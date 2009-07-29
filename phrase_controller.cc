/*******************************************************************************************
 *   phrase controller.cc
 *     brief: Phrase data consists of two parts, string data set and pointer data set.
 *            String data set is collection of raw phrase string and 
 *            pointer data set is collection of offset of string data that 
 *            corresponding.
 *            PHRASE_ID is equal to PHRASE_ADDRESS, then we cannot remove phrase.
 *            We can only detect not-referred data and regard as garbage.
 *            (Not implemented because garbage collection of phrase data rarely needs.)
 *
 *            phrase controller.cc : pointer data
 *            phrase_data_controller: raw string data
 *
 *   $Author: imamura_shoichi $
 *   $Date:: 2009-04-08 11:46:34 +0900#$
 *
 *   Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ********************************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <iostream>
#include "phrase_controller.h"


//////////////////////////////////////////
//  constructor & destuctor
//////////////////////////////////////////
PhraseController::PhraseController() {
  phrase_data = NULL;
  data = NULL;
  info = NULL;
  shm  = NULL;
}

PhraseController::~PhraseController() {
  phrase_data = NULL;
  data = NULL;
  info = NULL;
  shm  = NULL;
}


//////////////////////////////////////////
//  public methods
//////////////////////////////////////////
bool PhraseController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  base_path = std::string(path);
  shm = _shm;
  header = &(shm->get_header()->p_header);

  data_file.set_file_name(path, DATA_TYPE_PHRASE_ADDR, "dat");
  data_file.set_shared_memory(shm);
  info_file.set_file_name(path, DATA_TYPE_PHRASE_INFO, "dat");
  info_file.set_shared_memory(shm);
 
  data_limit= shm->get_page_size() / sizeof(PhraseAddr);
  info_limit = shm->get_page_size() / sizeof(PhraseInfo);

  return true;
}

bool PhraseController::init(std::string path, SharedMemoryAccess* _shm) {
  if(!setup(path, _shm)) return false;
  return init();
}

bool PhraseController::init() {
  if(!clear()) return false;
  if(!init_data()) return false;
  if(!init_info()) return false;

  return true;
}

bool PhraseController::clear() {
  clear_data();
  clear_info();

  if(phrase_data) phrase_data->clear();
  data_file.remove_with_suffix();
  info_file.remove_with_suffix();

  return true;
}

bool PhraseController::reset() {
  clear_data();
  clear_info();
  if(phrase_data && !phrase_data->reset()) return false;
 
  return true;
}

bool PhraseController::save() {
  if(phrase_data && !phrase_data->save()) return false;

  if(!save_data()) return false;
  if(!save_info()) return false;
  return true;
}



bool PhraseController::finish() {
  clear_data();
  clear_info();
  if(phrase_data) phrase_data->finish();

  return true;
}


///////////////////////////////////////////////////////////
//  search
///////////////////////////////////////////////////////////
PhraseData PhraseController::find_data(PhraseAddr addr, Buffer& buf) {
  return find_data_by_addr(addr, buf);
}


PhraseData PhraseController::find_data_by_addr(PhraseAddr addr, Buffer& buf) {
  return phrase_data->find(addr, buf);
}

PhraseData PhraseController::find_data(PhraseAddr addr) {
  return find_data_by_addr(addr);
}


PhraseData PhraseController::find_data_by_addr(PhraseAddr addr) {
  return phrase_data->find(addr);
}

PhraseAddr PhraseController::find_addr(const char* str) {
  return find_addr_by_string(str);
}

PhraseAddr PhraseController::find_addr_by_string(const char* str) {
  int len = strlen(str)+2;
  char buf[len];
  strcpy(buf+1, str);
  buf[0] = CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING);
  PhraseData d = {buf};

  return find_addr(d);
}

PhraseAddr PhraseController::find_addr(PhraseData d) {
  PhraseAddr err = {0, NULL_PHRASE};
  PhraseInfo info = find_info(d);
  int l=-1, r=info.count;
  int mid;

  if(info.pageno < 0 || !load_data(info.pageno, PAGE_READONLY)) goto find_error;

  while(r-l > 1) {
    mid = (l+r)/2;
    PhraseData d2 = find_data(data[mid]);
    if(!d.value) goto find_error;

    int cmp = phrase_data_comp(d, d2); 
    if(cmp > 0)         l=mid;
    else if(cmp == 0)   {
      PhraseAddr a = data[mid];
      return a;
    }
    else                r=mid;
  }

find_error:
  return err;
}



////////////////////////////////////////////////////////////////////
//  insert
////////////////////////////////////////////////////////////////////
PhraseAddr PhraseController::insert(PhraseData d) {
  INSERT_PHRASE_SET ins_set;
  InsertPhrase ins = {0, 0, d, {0, NULL_PHRASE}};
  ins_set.push_back(ins);
  insert(ins_set);

  return ins_set[0].addr;
}


PhraseAddr PhraseController::insert(const char* str) {
  PhraseAddr err = {0, NULL_PHRASE};
  if(str == NULL) return err;
  unsigned int len = strlen(str);
  if(len == 0) return err;

  PhraseData d;
  d.value = (char*)malloc(len+1+sizeof(char));
  d.value[0] = CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING);
  strcpy(d.value+1, str);
  PhraseAddr addr = insert(d);
  free(d.value);

  return addr;
}


PhraseAddr PhraseController::insert(const std::string& str) {
  PhraseAddr err = {0, NULL_PHRASE};
  if(str == "") return err;

  return insert(str.c_str());
}

PhraseAddr PhraseController::insert(int var) {
  INSERT_PHRASE_SET ins_set;
  InsertPhrase ins;
  ins.data.value = (char*)malloc(sizeof(int)+sizeof(char));
  ins.data.value[0] = CREATE_ATTR_HEADER(0, ATTR_TYPE_INTEGER);
  *((int*)(ins.data.value+1)) = var;
  ins_set.push_back(ins); 
  insert(ins_set);
  free(ins.data.value);

  return ins_set[0].addr;
}


bool PhraseController::insert(INSERT_PHRASE_SET& phrases) {
  if(phrases.size() == 0) return true;
  
  RANGE r = RANGE(0, phrases.size()-1);
  PHRASE_INFO_SET new_info = insert_info(phrases, header->root, r);
  if(new_info.size() > 1) {
    info_file.add_page(&new_info[0], 0, header->next_info, sizeof(PhraseInfo)*new_info.size());
    (header->root).count = new_info.size();
    (header->root).pageno = header->next_info;
    (header->root).level++;
    (header->root).max_addr = new_info[new_info.size()-1].max_addr;
    (header->next_info)++;
  } else {
    header->root = new_info[0];
  }
  save();

  return true;
}


PHRASE_INFO_SET PhraseController::insert_info(INSERT_PHRASE_SET& phrases, PhraseInfo current, RANGE r) {
  PHRASE_INFO_SET after;
  PHRASE_INFO_SET insert;

  MergeData m = {RANGE(0, current.count-1), r};

  load_info(current.pageno, PAGE_READONLY);
  MERGE_SET md = get_insert_info(phrases, m);
  MERGE_SET mi;

  for(unsigned int i=0; i<md.size(); i++) {
    if(md[i].src.first != md[i].src.second) continue;

    PHRASE_INFO_SET partial;
    MergeData m = {md[i].src};
    save_info();
    load_info(current.pageno, PAGE_READONLY);
    PhraseInfo the_info = info[md[i].src.first];

    try {
      if(the_info.level > 0) {
        partial = insert_info(phrases, the_info, md[i].dst);
      } else {
        partial = insert_data(phrases, the_info, md[i].dst);
      }
    } catch(AppException e) {
      write_log(LOG_LEVEL_ERROR, e.what(), "");
      partial.push_back(the_info);
    }
    if(partial.size() == 0) continue;


    m.dst.first = insert.size();
    m.dst.second = insert.size() + partial.size() - 1;
    for(unsigned int j=0; j<partial.size(); j++) {
      insert.push_back(partial[j]);
    }
    mi.push_back(m);
  }

  save_info();
  load_info(current.pageno, PAGE_READWRITE);
  unsigned int insert_count = insert.size();
  unsigned int total_count = current.count + insert_count;
  if(total_count <= info_limit) {
    memmove(info+insert_count, info, sizeof(PhraseInfo)*current.count);
    total_count = merge_info(info, info+insert_count, insert, mi, current.count);

    PhraseInfo new_info = current;
    new_info.count = total_count;
    after.push_back(new_info);
  } else {
    PhraseInfo* wbuf = (PhraseInfo*)malloc(sizeof(PhraseInfo)*total_count);
    total_count = merge_info(wbuf, info, insert, mi, current.count);
    if(total_count > info_limit) {
      after = split_info(wbuf, total_count, current.pageno);
    } else {
      memcpy(info, wbuf, sizeof(PhraseInfo)*total_count);
      PhraseInfo new_info = current;
      new_info.count = total_count;
      after.push_back(new_info);
    }
    free(wbuf);
  }

  return after;
}


PHRASE_INFO_SET PhraseController::insert_data(INSERT_PHRASE_SET& phrases, PhraseInfo current, RANGE r) {
  PHRASE_INFO_SET after_insert;

  if(!phrase_data) return after_insert;
  if(!load_data(current.pageno, PAGE_READWRITE)) return after_insert;

  MergeData init;
  init.src.first = 0, init.src.second = current.count-1;
  init.dst = r;
  MERGE_SET merge = get_insert_data(phrases, init);

  unsigned int insert_count = r.second - r.first + 1;
  unsigned int total_count =  current.count + insert_count;

  if(total_count <= data_limit) {
    memmove(data+insert_count, data, sizeof(PhraseAddr)*current.count);
    total_count = merge_data(data, data+insert_count, phrases, merge);

    PhraseInfo new_info = current;
    new_info.count = total_count;
    after_insert.push_back(new_info);
  } else {
    PhraseAddr* wbuf = (PhraseAddr*)malloc(sizeof(PhraseAddr)*total_count);
    total_count = merge_data(wbuf, data, phrases, merge);
    if(total_count > data_limit) {
      after_insert = split_data(wbuf, total_count, current.pageno);
    } else {
      memcpy(data, wbuf, sizeof(PhraseAddr)*total_count);
      PhraseInfo new_info = current;
      new_info.count = total_count;
      after_insert.push_back(new_info);
    }
    free(wbuf);
  }

  // keep maximum point
  if(after_insert.size() > 0) after_insert[after_insert.size()-1].max_addr = current.max_addr;

  return after_insert;
}


unsigned int PhraseController::merge_data
(PhraseAddr* wbuf, PhraseAddr* rbuf, INSERT_PHRASE_SET& phrases, MERGE_SET& merges) {
  int read_pos = 0;
  int write_pos = 0;

  PhraseAddr tail_addr = {0, NULL_PHRASE};
  for(unsigned int i=0; i<merges.size(); i++) {
    if(merges[i].dst.first != -1 && merges[i].dst.second != -1) {
      int ins_pos = merges[i].dst.first;
      while(ins_pos <= merges[i].dst.second) {
        // std::cout << "insert" << ins_pos << "\n";
        if(phrases[ins_pos].addr.offset != NULL_PHRASE) {
          tail_addr = phrases[ins_pos].addr;
          ins_pos++;
          continue;
        }
        phrases[ins_pos].addr = phrase_data->insert(phrases[ins_pos]);
        tail_addr = phrases[ins_pos].addr;
        wbuf[write_pos++] = phrases[ins_pos++].addr;
      }
    }

    if(merges[i].src.first != -1 && merges[i].src.second != -1) {
      while((int)read_pos <= merges[i].src.second && phrase_addr_comp(rbuf[read_pos], tail_addr) == 0) read_pos++;
      if((int)read_pos <= merges[i].src.second) {
        memmove(wbuf+write_pos, rbuf+read_pos, sizeof(PhraseAddr)*(merges[i].src.second-read_pos+1));
        write_pos = write_pos + (merges[i].src.second-read_pos+1);
        read_pos  = merges[i].src.second + 1;
      }
      // while((int)read_pos <= merges[i].src.second) wbuf[write_pos++] = rbuf[read_pos++];
    }
  }

  return write_pos;
}

unsigned int PhraseController::merge_info
(PhraseInfo* wbuf, PhraseInfo* rbuf, PHRASE_INFO_SET& infos, MERGE_SET& merges, int count)  {
  int read_pos = 0;
  int write_pos = 0;

  for(unsigned int i=0; i<merges.size(); i++) {
    if(merges[i].src.first != merges[i].src.second) continue;

    while((int)read_pos <= merges[i].src.first) {
      wbuf[write_pos++] = rbuf[read_pos++];
    }
    if(write_pos > 0) write_pos--;

    int ins_pos = merges[i].dst.first;
    while(ins_pos <= merges[i].dst.second) {
      // std::cout << "insert" << ins_pos << "\n";
      wbuf[write_pos++] = infos[ins_pos++];
    }
  }

  if(read_pos < count) {
    memcpy(wbuf+write_pos, rbuf+read_pos, sizeof(PhraseInfo)*(count-read_pos));
    write_pos = write_pos + count-read_pos;
  }

  return write_pos;
}




MERGE_SET PhraseController::get_insert_info(INSERT_PHRASE_SET& phrases, MergeData init) {
  MERGE_SET result;
  MERGE_SET work;

  if(!info) return result;

  work.push_back(init);
  while(work.size() > 0) {
    MergeData m = work[work.size()-1];
    work.pop_back();

    if(m.src.first == m.src.second) {
      result.push_back(m);
    } else if(m.src.first < m.src.second) {
      int srcmid = (m.src.first + m.src.second)/2;
      int dstmid = (m.dst.first + m.dst.second)/2;
      int l = m.dst.first - 1;
      int r = m.dst.second+ 1;

      // if move l the element accept, otherwise reject.
      while(r-l > 1) {
        dstmid = (l+r)/2;

        if(info[srcmid].max_addr.sector == DATA_SECTOR_LEFT) {
          r = dstmid;
        } else if(info[srcmid].max_addr.sector == DATA_SECTOR_RIGHT) {
          l = dstmid;
        } else {
          PhraseData max_data = phrase_data->find(info[srcmid].max_addr);
          if(phrase_data_comp(max_data, phrases[dstmid].data) < 0)  r=dstmid;
          else                                                      l=dstmid;
        }
      }
      dstmid = l;

      MergeData left_set, right_set;
      left_set.src.first  = m.src.first;
      left_set.src.second = srcmid;
      right_set.src.first = srcmid+1;
      right_set.src.second= m.src.second;

      left_set.dst.first  = m.dst.first;
      left_set.dst.second = dstmid;
      right_set.dst.first = dstmid+1;
      right_set.dst.second= m.dst.second;

      if(right_set.dst.first <= right_set.dst.second) work.push_back(right_set);
      if(left_set.dst.first  <= left_set.dst.second)  work.push_back(left_set);
    }
  }

  return result;
}


MERGE_SET PhraseController::get_insert_data(INSERT_PHRASE_SET& phrases, MergeData init) {
  MERGE_SET empty;
  MERGE_SET insert_info;
  MERGE_SET work;

  MergeData work_init = init;
  if(work_init.src.first < 0 || work_init.src.second < 0)  { // empty source
    work_init.src.first = work_init.src.second = -1;
    insert_info.push_back(work_init);
    return insert_info;
  }

  work_init.src.second++;  // tail data prepare
  work.push_back(work_init);

  while(work.size() > 0) {
    MergeData m = work[work.size()-1];
    work.pop_back();

    if(m.dst.first < 0 || m.dst.second < 0 || m.src.first == m.src.second) {  // no match data
      if(m.dst.first < 0 || m.dst.second < 0) {
        m.dst.first = m.dst.second = -1;
      }
      if(m.src.second > init.src.second) { // tail fix
        if(m.src.second-1 >= m.src.first) m.src.second--;
        else                              m.src.first = m.src.second = -1;
      }
      insert_info.push_back(m);
      continue;
    }


    int srcmid = (m.src.first + m.src.second)/2;
    RANGE dst_range = get_match_data(phrases, srcmid, m.dst);

    MergeData left_set, right_set;
    left_set.dst.first  = m.dst.first;
    left_set.dst.second = dst_range.first;
    right_set.dst.first = dst_range.second;
    right_set.dst.second= m.dst.second;

    left_set.src.first  = m.src.first;
    left_set.src.second = srcmid;
    right_set.src.first = srcmid+1;
    right_set.src.second= m.src.second;

    if(right_set.dst.first > right_set.dst.second) {
      right_set.dst.first = right_set.dst.second = -1;
    }
    if(left_set.dst.first  > left_set.dst.second)  {
      left_set.dst.first = left_set.dst.second = -1;
    }
    work.push_back(right_set);
    work.push_back(left_set);
  }

  // optimize
  unsigned int opt_read = 0, opt_write = 0;
  while(opt_read < insert_info.size()) {
    if(opt_write == 0 || insert_info[opt_read].dst.first != -1) {
      insert_info[opt_write] = insert_info[opt_read];
      opt_write++;
    } else if(insert_info[opt_read].src.first != -1) {
      insert_info[opt_write-1].src.second = insert_info[opt_read].src.second;
    }

    opt_read++;
  }
  insert_info.resize(opt_write);



/*
  std::cout << "===\n";
  for(unsigned int i=0; i<insert_info.size(); i++) {
    std::cout << insert_info[i].src.first << "," << insert_info[i].src.second << ":" << insert_info[i].dst.first << "," << insert_info[i].dst.second << "\n";
  }
*/

  return insert_info;
}


RANGE PhraseController::get_match_data(INSERT_PHRASE_SET& phrases, unsigned int srcmid, RANGE range) {
  RANGE dst_range;
  int l, r;
  PhraseData d = phrase_data->find(data[srcmid]);

  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    int cmp = phrase_data_comp(phrases[dstmid].data, d);
    if(cmp==0) phrases[dstmid].addr = data[srcmid];

    if(cmp<0) l = dstmid;
    else      r = dstmid;
  }
  dst_range.first  = l;
  dst_range.second = r;

/*
  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    if(phrases[dstmid].data.id == d.id) phrases[dstmid].addr = data[srcmid];

    if(phrases[dstmid].data.id > d.id) r = dstmid;
    else                            l = dstmid;
  }
  dst_range.second = r;
*/

  return dst_range;
}

void PhraseController::set_phrase_data(PhraseDataController* p) {
  phrase_data   = p;
}



///////////////////////////////////////////////
// private methods
///////////////////////////////////////////////
bool PhraseController::save_data() {
  data_file.save_page();
  data = NULL;

  return true;
}

bool PhraseController::save_info() {
  info_file.save_page();
  info = NULL;

  return true;
}


bool PhraseController::clear_data() {
  data_file.clear_page();
  data = NULL;

  return true;
}

bool PhraseController::clear_info() {
  info_file.clear_page();
  info = NULL;

  return true;
}

bool PhraseController::load_data(unsigned int pageno, int mode) {
  data = (PhraseAddr*)data_file.load_page(0, pageno, mode);
  if(!data) return false;

  return true;
}

bool PhraseController::load_info(unsigned int pageno, int mode) {
  info = (PhraseInfo*)info_file.load_page(0, pageno, mode);
  if(!info) return false;

  return true;
}

PHRASE_INFO_SET PhraseController::split_data(PhraseAddr* wbuf, unsigned int count, int pageno) {
  PHRASE_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(data, wbuf+left_pos, sizeof(PhraseAddr)*(right_pos - left_pos));
  PhraseInfo first_info = {(right_pos-left_pos), pageno, 0, wbuf[right_pos-1]};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    data_file.add_page(wbuf+left_pos, 0, header->next_data, sizeof(PhraseAddr)*(right_pos-left_pos));
    PhraseInfo add_info = {(right_pos-left_pos), header->next_data, 0, wbuf[right_pos-1]};
    after_info.push_back(add_info);
    header->next_data++;
  }

  return after_info;
}


PHRASE_INFO_SET PhraseController::split_info(PhraseInfo* wbuf, unsigned int count, int pageno) {
  PHRASE_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(info, wbuf+left_pos, sizeof(PhraseInfo)*(right_pos - left_pos));
  PhraseInfo first_info = {(right_pos-left_pos), pageno, 1, wbuf[right_pos-1].max_addr};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    info_file.add_page(wbuf+left_pos, 0, header->next_info, sizeof(PhraseInfo)*(right_pos-left_pos));
    PhraseInfo add_info = {(right_pos-left_pos), header->next_info, 1, wbuf[right_pos-1].max_addr};
    after_info.push_back(add_info);
    header->next_info++;
  }

  return after_info;
}

bool PhraseController::init_data() {
  data_file.add_page(NULL, 0, 0);
  header->next_data = 1;

  return true;
}

bool PhraseController::init_info() {
  PHRASE_INFO_SET init;
  PhraseInfo first_info = {0, 0, 0,  {DATA_SECTOR_LEFT, NULL_PHRASE}};
  PhraseInfo second_info = {0, 0, 0,  {DATA_SECTOR_RIGHT, NULL_PHRASE}};
  init.push_back(first_info);
  init.push_back(second_info);
  info_file.add_page(&init[0], 0, 0, sizeof(PhraseInfo)*sizeof(init));
  header->next_info = 1;

  (header->root).max_addr.sector = DATA_SECTOR_RIGHT;
  (header->root).max_addr.offset = NULL_PHRASE;
  (header->root).count  = 2;
  (header->root).pageno = 0;
  (header->root).level  = 1;

  return true;
}


PhraseInfo PhraseController::find_info(PhraseData pdata) {
  PhraseInfo err    = {0, 0, -1};
  PhraseInfo current = header->root;

  while(1) {
    if(current.pageno < 0 || !load_info(current.pageno, PAGE_READONLY)) return err;

    int l = -1, r = current.count;
    while(r-l > 1) {
      int mid = (l+r)/2;

      if(info[mid].max_addr.sector == DATA_SECTOR_LEFT) {
        l = mid;
      } else if(info[mid].max_addr.sector == DATA_SECTOR_RIGHT) {
        r = mid;
      } else {
        PhraseData max_data = phrase_data->find(info[mid].max_addr);
        if(phrase_data_comp(pdata, max_data) > 0) l = mid;
        else                                      r = mid;
      }
    }

    current = info[r];
    if(current.level == 0)  break;
  }

  return current;
}



/////////////////////////////////////////////////
// for debug
/////////////////////////////////////////////////
bool PhraseController :: test() { 
  unsigned int prev_data_limit = data_limit;
  unsigned int prev_info_limit = info_limit;
  data_limit = 8;
  info_limit = 8;

  PHRASE_DATA_SET str_sample, num_sample;
  Buffer buf;
  for(unsigned int i=0; i<1000; i++) {
    char s[15];
    sprintf(s, "sample%05d", i);

    char* str = buf.allocate(strlen(s)+2);
    str[0] = CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING);
    strcpy(str+1, s);
    PhraseData d = {str};
    str_sample.push_back(d);
  }

  for(unsigned int i=0; i<1000; i++) {
    int val = i+10000;
    char* str = buf.allocate(1+sizeof(int));
    str[0] = CREATE_ATTR_HEADER(0, ATTR_TYPE_INTEGER);
    memcpy(str+1, &val, sizeof(int));
    PhraseData d = {str};
    num_sample.push_back(d);
  } 

  std::cout << "sequential insert test...\n";
  shm->init();
  init();
  for(unsigned int i=0; i<str_sample.size(); i++) {
    insert(str_sample[i]);
  }

  for(unsigned int i=0; i<str_sample.size(); i++) {
    PhraseAddr addr = find_addr(str_sample[i]);
    PhraseData data = find_data(addr, buf);
    if(phrase_data_comp(data, str_sample[i]) != 0) return false;
  }

  std::cout << "reverse insert test...\n";
  shm->init();
  init();
  for(int i=str_sample.size()-1; i>=0; i--) {
    insert(str_sample[i]);
  }

  for(unsigned int i=0; i<str_sample.size(); i++) {
    PhraseAddr addr = find_addr(str_sample[i]);
    PhraseData data = find_data(addr, buf);
    if(phrase_data_comp(data, str_sample[i]) != 0) return false;
  }


  std::cout << "same phrase test...\n";
  for(unsigned int i=0; i<str_sample.size(); i++) {
    insert(str_sample[i]);
  }

  for(unsigned int i=0; i<str_sample.size(); i++) {
    PhraseAddr addr = find_addr(str_sample[i]);
    PhraseData data = find_data(addr, buf);
    if(phrase_data_comp(data, str_sample[i]) != 0) return false;
  }

  std::cout << "insert number test...\n";
  for(unsigned int i=0; i<num_sample.size(); i++) {
    insert(num_sample[i]);
  }

  for(unsigned int i=0; i<num_sample.size(); i++) {
    PhraseAddr addr = find_addr(num_sample[i]);
    PhraseData data = find_data(addr, buf);
    if(phrase_data_comp(data, num_sample[i]) != 0) return false;
  }

  std::cout << "multiple insert test...\n";
  INSERT_PHRASE_SET phrases;
  shm->init();
  init();

  phrases.clear();
  for(unsigned int i=0; i<str_sample.size(); i=i+2) {
    InsertPhrase p = {0, 0, str_sample[i], {0, NULL_PHRASE}};
    phrases.push_back(p);
  }
  insert(phrases);
  

  phrases.clear();
  for(unsigned int i=1; i<str_sample.size(); i=i+2) {
    InsertPhrase p = {0, 0, str_sample[i], {0, NULL_PHRASE}};
    phrases.push_back(p);
  }
  insert(phrases);

  for(unsigned int i=0; i<str_sample.size(); i++) {
    PhraseAddr addr = find_addr(str_sample[i]);
    PhraseData data = find_data(addr, buf);
    if(phrase_data_comp(data, str_sample[i]) != 0) return false;
  }


  std::cout << "huge data test...\n";
  data_limit = prev_data_limit;
  info_limit = prev_info_limit;

  shm->init();
  init();

  for(unsigned int i=0; i<100000; i++) {
    if(i%10000 == 0) std::cout << i << "...\n";

    char str[30];
    sprintf(str, "hugephrase%6d", i);
    PhraseAddr addr = insert(str);
    if(addr.offset == NULL_PHRASE) {
      std::cout << "failed\n";
      return false;
    }
  }

  std::cout << "end process...\n";
  finish();

  buf.clear();
  return true;
}



void PhraseController::dump() {
  std::cout << "---dump start\n";
  dump(header->root);
  std::cout << "---dump end\n";
}

void PhraseController::dump(PhraseInfo current) {
  if(current.level > 0) {
    for(unsigned int i=0; i<current.count; i++) {
      load_info(current.pageno, PAGE_READONLY);
      std::cout << "====node " << i << "(pageno " << info[i].pageno << ", count" << info[i].count <<  ") ====\n";
      dump(info[i]);
    }
  } else {
    load_data(current.pageno, PAGE_READONLY);
    for(unsigned int i=0; i<current.count; i++) {
      PhraseData d = find_data(data[i]);

      unsigned char type = (unsigned char)d.value[0];
      const void* val = d.value + 1;
      if(IS_ATTR_TYPE_STRING(type)) std::cout << "  " << (const char*)val << "(string:" <<  (unsigned int)(type & 0x7F) << ")\n";
      else                          std::cout << "  " << *((const int*)val) << "(integer:" <<  (unsigned int)(type & 0x7F) << ")\n";
    }
  }
}

