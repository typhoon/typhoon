/*****************************************************************
 *  regular_index_controller.cc
 *     brief: Document-phrase mapping data.
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 19:53:02 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include "regular_index_controller.h"

/////////////////////////////////////////////
// constructor & destructor
/////////////////////////////////////////////
RegularIndexController::RegularIndexController() {
  data = NULL;
  document = NULL;
  phrase = NULL;
  info = NULL;
  header = NULL;
}

RegularIndexController::~RegularIndexController() {
  data = NULL;
  document = NULL;
  phrase = NULL;
  info = NULL;
  header = NULL;
}



/////////////////////////////////////////////
// public methods
/////////////////////////////////////////////
bool RegularIndexController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  base_path = path;
  shm = _shm;

  header = &(shm->get_header()->reg_header);
  data_file.set_file_name(path, DATA_TYPE_REGULAR_INDEX, "dat");
  data_file.set_shared_memory(shm);
  info_file.set_file_name(path, DATA_TYPE_REGULAR_INDEX_INFO, "dat");
  info_file.set_shared_memory(shm); 

  data_limit = shm->get_page_size() / sizeof(RegularIndex);
  info_limit = shm->get_page_size() / sizeof(RegularIndexInfo);

  return true;
}

bool RegularIndexController::init(std::string path, SharedMemoryAccess* _shm) {
  if(!setup(path, _shm)) return false;
  return init();
}

bool RegularIndexController::init() { // without settings
  if(!clear()) return false;
  if(!init_data()) return false;
  if(!init_info()) return false;

  return true;
}

bool RegularIndexController::clear() {
  clear_data();
  clear_info();

  data_file.remove_with_suffix();
  info_file.remove_with_suffix();

  return true;
}

bool RegularIndexController::reset() {
  clear_data();
  clear_info();

  return true;
}

bool RegularIndexController::save() {
  if(!save_data()) return false;
  if(!save_info()) return false;
  return true;
}


bool RegularIndexController::finish() {
  clear_data();
  clear_info();

  return true;
}



void RegularIndexController::set_document_and_phrase(DocumentController* doc, PhraseController* ph) {
  document = doc;
  phrase = ph;
}



/////////////////////////////////////////////////////////////////////////////
//  insert
/////////////////////////////////////////////////////////////////////////////
bool RegularIndexController::insert(INSERT_REGULAR_INDEX_SET& indexes) {
  if(indexes.size() == 0) return true;

  RANGE r = RANGE(0, indexes.size()-1);
  REGULAR_INDEX_INFO_SET new_info;

  new_info = insert_info(indexes, header->root, r);
  levelup_root_info(new_info);
  save(); 
 
  return true;
} 


void RegularIndexController::levelup_root_info(REGULAR_INDEX_INFO_SET& new_info) {
  if(new_info.size() > 1) {
    info_file.add_page(&new_info[0], 0, header->next_info, sizeof(RegularIndexInfo)*new_info.size());
    (header->root).count = new_info.size();
    (header->root).pageno = header->next_info;
    (header->root).level++;
    (header->root).max_addr = new_info[new_info.size()-1].max_addr;
    (header->next_info)++;
  } else {
    header->root = new_info[0];
  }
}


REGULAR_INDEX_INFO_SET RegularIndexController::insert_info(INSERT_REGULAR_INDEX_SET& indexes, RegularIndexInfo current, RANGE r) {
  REGULAR_INDEX_INFO_SET after;
  REGULAR_INDEX_INFO_SET insert;

  MergeData m = {RANGE(0, current.count-1), r};

  load_info(current.pageno, PAGE_READONLY);
  MERGE_SET md = get_insert_info(indexes, m);
  MERGE_SET mi;

  for(unsigned int i=0; i<md.size(); i++) {
    if(md[i].src.first != md[i].src.second) continue;

    REGULAR_INDEX_INFO_SET partial;
    MergeData m = {md[i].src};
 
    save_info();
    load_info(current.pageno, PAGE_READONLY);
    RegularIndexInfo the_info = info[md[i].src.first];
   
    try { 
      if(the_info.level > 0) {
        partial = insert_info(indexes, the_info, md[i].dst);
      } else {
        partial = insert_data(indexes, the_info, md[i].dst);
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
    memmove(info+insert_count, info, sizeof(RegularIndexInfo)*current.count);
    total_count = merge_info(info, info+insert_count, insert, mi, current.count);

    RegularIndexInfo new_info = current;
    new_info.count = total_count;
    after.push_back(new_info);
  } else {
    RegularIndexInfo* wbuf = (RegularIndexInfo*)malloc(sizeof(RegularIndexInfo)*total_count);
    total_count = merge_info(wbuf, info, insert, mi, current.count);
    if(total_count > info_limit) {
      after = split_info(wbuf, total_count, current.pageno);
    } else {
      memcpy(info, wbuf, sizeof(RegularIndexInfo)*total_count);
      RegularIndexInfo new_info = current;
      new_info.count = total_count;
      after.push_back(new_info);
    }
    free(wbuf);
  }

  return after;
}


REGULAR_INDEX_INFO_SET RegularIndexController::insert_data(INSERT_REGULAR_INDEX_SET& indexes, RegularIndexInfo current, RANGE r) {
  REGULAR_INDEX_INFO_SET after_insert;
  if(!load_data(current.pageno, PAGE_READWRITE)) return after_insert;

  MergeData init;
  init.src.first = 0, init.src.second = current.count-1;
  init.dst = r;
  MERGE_SET merge = get_insert_data(indexes, init);

  unsigned int insert_count = 0;
  for(int i=r.first; i<=r.second; i++) {
    insert_count = insert_count + indexes[i].phrases.size() + (2*indexes[i].phrases.size())/MAX_REGULAR_INDEX_BLOCK + 4;
  }
  unsigned int total_count =  current.count + insert_count;

  if(total_count <= data_limit) {
    memmove(data+insert_count, data, sizeof(RegularIndex)*current.count);
    total_count = merge_data(data, data+insert_count, indexes, merge);

    RegularIndexInfo new_info = current;
    new_info.count = total_count;
    after_insert.push_back(new_info);
  } else {
    RegularIndex* wbuf = (RegularIndex*)malloc(sizeof(RegularIndex)*total_count);
    total_count = merge_data(wbuf, data, indexes, merge);
    if(total_count > data_limit) {
      after_insert = split_data(wbuf, total_count, current.pageno);
    } else {
      memcpy(data, wbuf, sizeof(RegularIndex)*total_count);
      RegularIndexInfo new_info = current;
      new_info.count = total_count;
      after_insert.push_back(new_info);
    }
    free(wbuf);
  }

  // keep maximum point
  if(after_insert.size() > 0) after_insert[after_insert.size()-1].max_addr = current.max_addr;
  return after_insert;
}



unsigned int RegularIndexController::merge_data
(RegularIndex* wbuf, RegularIndex* rbuf, INSERT_REGULAR_INDEX_SET& indexes, MERGE_SET& merges) {
  // merge
  unsigned int read_pos = 0;
  unsigned int write_pos = 0;

  DocumentAddr tail_addr = {0, NULL_DOCUMENT};
  for(unsigned int i=0; i<merges.size(); i++) {
    if(merges[i].dst.first != -1 && merges[i].dst.second != -1) {
      int ins_pos = merges[i].dst.first;
      while(ins_pos <= merges[i].dst.second) {
        unsigned short the_sector = DATA_SECTOR_LEFT;
        unsigned int ofs = 0;
        //std::cout << "insert" << ins_pos << "\n";
        for(unsigned int j=0; j<indexes[ins_pos].phrases.size(); j++) {
          if(the_sector != indexes[ins_pos].phrases[j].addr.sector) {
            wbuf[write_pos++].val = CREATE_REG_INDEX_HEADER_FIRST(0, indexes[ins_pos].doc.addr.offset);
            wbuf[write_pos++].val = CREATE_REG_INDEX_HEADER_SECOND(indexes[ins_pos].phrases[j].addr.sector, 
                                                                   indexes[ins_pos].doc.addr.sector);
            the_sector = indexes[ins_pos].phrases[j].addr.sector;
            ofs = 2;
          }
          else if(ofs > MAX_REGULAR_INDEX_BLOCK) {
            wbuf[write_pos++].val = CREATE_REG_INDEX_HEADER_FIRST(0, indexes[ins_pos].doc.addr.offset);
            wbuf[write_pos++].val = CREATE_REG_INDEX_HEADER_SECOND(indexes[ins_pos].phrases[j].addr.sector,
                                                                   indexes[ins_pos].doc.addr.sector);
            ofs = 2;
          }

          wbuf[write_pos++].val = CREATE_REG_INDEX_BODY_FIRST(indexes[ins_pos].phrases[j].weight, 
                                                              indexes[ins_pos].phrases[j].addr.offset);
          ofs++;
        } 

        tail_addr = indexes[ins_pos].doc.addr;
        ins_pos++;
      }
    }

    if(merges[i].src.first != -1) {
      DocumentAddr the_addr;
      while((int)read_pos <= merges[i].src.second) {
        if(IS_INDEX_HEADER_FIRST(rbuf[read_pos].val))  {
          the_addr = get_document_addr(rbuf, read_pos);
        }

        if(document_addr_comp(the_addr, tail_addr) == 0) {
          read_pos++;
          continue;
        }
        wbuf[write_pos++] = rbuf[read_pos++];
      }
    }
  }

  return write_pos; 
}

unsigned int RegularIndexController::merge_info
(RegularIndexInfo* wbuf, RegularIndexInfo* rbuf, REGULAR_INDEX_INFO_SET& infos, MERGE_SET& merges, int count)  {
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
    memcpy(wbuf+write_pos, rbuf+read_pos, sizeof(RegularIndexInfo)*(count-read_pos));
    write_pos = write_pos + count-read_pos;
  }

  return write_pos;
}


MERGE_SET RegularIndexController::get_insert_info(INSERT_REGULAR_INDEX_SET& indexes, MergeData init) {
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
          if(document_addr_comp(info[srcmid].max_addr, indexes[dstmid].doc.addr) < 0)  r=dstmid;
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



MERGE_SET RegularIndexController::get_insert_data(INSERT_REGULAR_INDEX_SET& indexes, MergeData init) {
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
    RANGE dst_range = get_match_data(indexes, srcmid, m.dst);

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

/*
  std::cout << "===\n";
  for(unsigned int i=0; i<insert_info.size(); i++) {     std::cout << insert_info[i].src.first << "," << insert_info[i].src.second << ":" << insert_info[i].dst.first << "," << insert_info[i].dst.second << "\n";
  }
*/
  
  return insert_info;
}



RANGE RegularIndexController::get_match_data(INSERT_REGULAR_INDEX_SET& indexes, unsigned int srcmid, RANGE range) {
  RANGE dst_range;
  int l, r;

  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    int cmp = document_addr_comp(indexes[dstmid].doc.addr, get_document_addr(data, srcmid));

    if(cmp<0) l = dstmid;
    else      r = dstmid;
  }
  dst_range.first  = l;
  dst_range.second = r;

/*
  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    if(indexes[dstmid].data.id == d.id) indexes[dstmid].addr = data[srcmid];

    if(indexes[dstmid].data.id > d.id) r = dstmid;
    else                            l = dstmid;
  }
  dst_range.second = r;
*/

  return dst_range;
}


/////////////////////////////////////////////////////////////////////////////
//  remove
/////////////////////////////////////////////////////////////////////////////
unsigned int RegularIndexController::remove(InsertRegularIndex& idx) {
  return remove_info(idx, header->root);
}

unsigned int RegularIndexController::remove_info(InsertRegularIndex& idx, RegularIndexInfo page_info) {
  int l, r;

  load_info(page_info.pageno, PAGE_READONLY);

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    int cmp = document_addr_comp(idx.doc.addr, info[dstmid].max_addr);
    if(cmp > 0) l = dstmid;
    else        r = dstmid;
  }

  int i = l+1;
  for(i=l+1; i<(int)page_info.count; i++) {
    load_info(page_info.pageno, PAGE_READWRITE);

    RegularIndexInfo the_info = info[i];
    if(info[i].level > 0) {
      remove_info(idx, the_info);
    } else {
      info[i].count = info[i].count-remove_data(idx, the_info);
    }
    if(document_addr_comp(idx.doc.addr, the_info.max_addr) != 0) break;
  }

  return 0;
}

unsigned int RegularIndexController::remove_data(InsertRegularIndex& idx, RegularIndexInfo page_info) {
  int l, r;
  int left_limit, right_limit;

  load_data(page_info.pageno, PAGE_READONLY);

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    int cmp = document_addr_comp(idx.doc.addr, get_document_addr(data, dstmid));
    if(cmp > 0) l= dstmid;
    else        r= dstmid;
  }
  left_limit = l+1;

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    int cmp = document_addr_comp(idx.doc.addr, get_document_addr(data, dstmid));
    if(cmp < 0) r= dstmid;
    else        l= dstmid;
  }
  right_limit = r-1;

  if(left_limit <= right_limit) {
    for(int i=left_limit; i<=right_limit; i++) {
      if(i<0 || i>= (int)page_info.count) continue;

      if(IS_INDEX_BODY(data[i].val)) {
        InsertPhrase p = {0, 0, {NULL}, get_phrase_addr(data, i)};
        idx.phrases.push_back(p);
      }
    }

    if(right_limit+1 < (int)page_info.count)  {
      memmove(data+left_limit, data+right_limit+1, sizeof(RegularIndex)*(page_info.count-left_limit+1));
    }
    return right_limit-left_limit+1;
  }

  return 0;
}





/////////////////////////////////////////////////////////////////////////////
//  find
/////////////////////////////////////////////////////////////////////////////
unsigned int RegularIndexController::find(DocumentAddr doc, PHRASE_ADDR_SET& phrases) {
  return find_info(doc, phrases, header->root);
}


unsigned int RegularIndexController::find_info(DocumentAddr doc, PHRASE_ADDR_SET& phrases, RegularIndexInfo page_info) {
  int l, r;

  load_info(page_info.pageno, PAGE_READONLY);

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    int cmp = document_addr_comp(doc, info[dstmid].max_addr);
    if(cmp > 0) l = dstmid;
    else        r = dstmid;
  }

  int i = l+1;
  for(i=l+1; i<(int)page_info.count; i++) {
    load_info(page_info.pageno, PAGE_READONLY);

    RegularIndexInfo the_info = info[i];
    if(info[i].level > 0) {
      find_info(doc, phrases, the_info);
    } else {
      find_data(doc, phrases, the_info);
    }

    if(document_addr_comp(doc, the_info.max_addr) != 0) break;
  }


  return phrases.size();  
}


unsigned int RegularIndexController::find_data(DocumentAddr doc, PHRASE_ADDR_SET& phrases, RegularIndexInfo page_info) {
  int l, r;
  int left_limit, right_limit;
  int count = 0;

  load_data(page_info.pageno, PAGE_READONLY);

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    int cmp = document_addr_comp(doc, get_document_addr(data, dstmid));
    if(cmp > 0) l= dstmid;
    else        r= dstmid;
  }
  left_limit = l+1;

  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    int cmp = document_addr_comp(doc, get_document_addr(data, dstmid));
    if(cmp < 0) r= dstmid;
    else        l= dstmid;
  }
  right_limit = r-1;


  for(int i=left_limit; i<=right_limit; i++) {
    if(i<0 || i>= (int)page_info.count) continue;

    if(IS_INDEX_BODY(data[i].val)) {
      phrases.push_back(get_phrase_addr(data, i));
      count++;
    }
  }
  return count;
}





///////////////////////////////////////////////
// private methods
///////////////////////////////////////////////
bool RegularIndexController::save_data() {
  data_file.save_page();
  data = NULL;

  return true;
}

bool RegularIndexController::save_info() {
  info_file.save_page();
  info = NULL;

  return true;
}


bool RegularIndexController::clear_data() {
  data_file.clear_page();
  data = NULL;

  return true;
}

bool RegularIndexController::clear_info() {
  info_file.clear_page();
  info = NULL;

  return true;
}

bool RegularIndexController::load_data(unsigned int pageno, int mode) {
  data = (RegularIndex*)data_file.load_page(0, pageno, mode);
  if(!data) return false;

  return true;
}

bool RegularIndexController::load_info(unsigned int pageno, int mode) {
  info = (RegularIndexInfo*)info_file.load_page(0, pageno, mode);
  if(!info) return false;

  return true;
}

REGULAR_INDEX_INFO_SET RegularIndexController::split_data(RegularIndex* wbuf, unsigned int count, int pageno) {
  REGULAR_INDEX_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-MAX_REGULAR_INDEX_BLOCK-2) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;
  int ofs = 0;
  while(!IS_INDEX_HEADER_FIRST(wbuf[right_pos-ofs].val)) {
    ofs++;
    if(right_pos <= left_pos+ofs) throw AppException(EX_APP_REGINDEX, "failed to split data");
  }
  right_pos = right_pos - ofs;

  // first page
  memcpy(data, wbuf+left_pos, sizeof(RegularIndex)*(right_pos - left_pos));
  RegularIndexInfo first_info = {(right_pos-left_pos), pageno, 0, get_document_addr(wbuf, right_pos-1)};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos + ofs;
    ofs = 0;

    if(right_pos >= count) right_pos = count;
    else {
      while(!IS_INDEX_HEADER_FIRST(wbuf[right_pos-ofs].val)) {
        ofs++;
        if(right_pos <= left_pos+ofs) throw AppException(EX_APP_REGINDEX, "failed to split data");
      }
      right_pos = right_pos - ofs;
    }

    data_file.add_page(wbuf+left_pos, 0, header->next_data, sizeof(RegularIndex)*(right_pos-left_pos));
    RegularIndexInfo add_info = {(right_pos-left_pos), header->next_data, 0, get_document_addr(wbuf, right_pos-1)};
    after_info.push_back(add_info);
    header->next_data++;
  }

  return after_info;
}


REGULAR_INDEX_INFO_SET RegularIndexController::split_info(RegularIndexInfo* wbuf, unsigned int count, int pageno) {
  REGULAR_INDEX_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(info, wbuf+left_pos, sizeof(RegularIndexInfo)*(right_pos - left_pos));
  RegularIndexInfo first_info = {(right_pos-left_pos), pageno, 1, wbuf[right_pos-1].max_addr};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    info_file.add_page(wbuf+left_pos, 0, header->next_info, sizeof(RegularIndexInfo)*(right_pos-left_pos));
    RegularIndexInfo add_info = {(right_pos-left_pos), header->next_info, 1, wbuf[right_pos-1].max_addr};
    after_info.push_back(add_info);
    header->next_info++;
  }

  return after_info;
}



bool RegularIndexController::init_data() {
  data_file.add_page(NULL, 0, 0);
  header->next_data = 1;

  return true;
}

bool RegularIndexController::init_info() {
  REGULAR_INDEX_INFO_SET init;
  RegularIndexInfo first_info = {0, 0, 0,  {DATA_SECTOR_LEFT, NULL_DOCUMENT}};
  RegularIndexInfo second_info = {0, 0, 0,  {DATA_SECTOR_RIGHT, NULL_DOCUMENT}};
  init.push_back(first_info);
  init.push_back(second_info);
  info_file.add_page(&init[0], 0, 0, sizeof(RegularIndexInfo)*sizeof(init));
  header->next_info = 1;

  (header->root).max_addr.sector = DATA_SECTOR_RIGHT;
  (header->root).max_addr.offset = NULL_DOCUMENT;
  (header->root).count  = 2;
  (header->root).pageno = 0;
  (header->root).level  = 1;

  return true;
}


DocumentAddr RegularIndexController::get_document_addr(RegularIndex* idx, unsigned int pos) {
  unsigned int ofs = 0;
  DocumentAddr result = {0, NULL_DOCUMENT};

  while(!IS_INDEX_HEADER_FIRST(idx[pos-ofs].val)) {
    if(ofs > pos || ofs > MAX_REGULAR_INDEX_BLOCK) {
      throw AppException(EX_APP_REGINDEX, "failed to get document addr");
    }
    ofs++;
  }

  result.offset = REG_INDEX_DOCUMENT_OFFSET(idx[pos-ofs].val);
  if(IS_INDEX_HEADER_SECOND(idx[pos-ofs+1].val)) {
    result.sector = REG_INDEX_DOCUMENT_SECTOR(idx[pos-ofs+1].val);
  } 

  return result;
}

PhraseAddr RegularIndexController::get_phrase_addr(RegularIndex* idx, unsigned int pos) {
  unsigned int ofs = 0;

  if(!IS_INDEX_BODY_FIRST(idx[pos].val)) {
    throw AppException(EX_APP_REGINDEX, "failed to get phrase addr");
  }

  while(!IS_INDEX_HEADER_FIRST(idx[pos-ofs].val)) {
    if(ofs > pos || ofs > MAX_REGULAR_INDEX_BLOCK) {
      throw AppException(EX_APP_REGINDEX, "failed to get phrase addr");
    }
    ofs++;
  }

  return get_phrase_addr(idx, pos, pos-ofs);
}

PhraseAddr RegularIndexController::get_phrase_addr(RegularIndex* idx, unsigned int phrase_pos, unsigned int doc_pos) {
  PhraseAddr result = {0, NULL_PHRASE};

  if(!IS_INDEX_BODY(idx[phrase_pos].val) || !IS_INDEX_HEADER_FIRST(idx[doc_pos].val)) {
    throw AppException(EX_APP_REGINDEX, "failed to get phrase addr");
  }
  
  result.offset = REG_INDEX_PHRASE_OFFSET(idx[phrase_pos].val);
  if(IS_INDEX_HEADER_SECOND(idx[doc_pos+1].val)) {
    result.sector = REG_INDEX_PHRASE_SECTOR(idx[doc_pos+1].val);
  }

  return result;
}




/////////////////////////////////////////////////
// for debug
/////////////////////////////////////////////////
bool RegularIndexController::test() {
  INSERT_DOCUMENT_SET docs;
  INSERT_PHRASE_SET   phrases;
  INSERT_REGULAR_INDEX_SET indexes;
  InsertRegularIndex x;
  PHRASE_ADDR_SET result_phrase;

  data_limit = (MAX_REGULAR_INDEX_BLOCK + 2)*2; // minimum
  info_limit = 10;

  std::cout << "macro test...\n";
  RegularIndex idx[10];
  DocumentAddr d = {10, 100};
  PhraseAddr   p1 = {20, 200};
  PhraseAddr   p2 = {30, 300};
  idx[0].val = CREATE_REG_INDEX_HEADER_FIRST(1, d.offset);
  idx[1].val = CREATE_REG_INDEX_HEADER_SECOND(p1.sector, d.sector);
  idx[2].val = CREATE_REG_INDEX_BODY_FIRST(1, p1.offset);
  idx[3].val = CREATE_REG_INDEX_HEADER_FIRST(1, d.offset);
  idx[4].val = CREATE_REG_INDEX_HEADER_SECOND(p2.sector, d.sector);
  idx[5].val = CREATE_REG_INDEX_BODY_FIRST(1, p2.offset);
  if(!IS_INDEX_HEADER_FIRST(idx[0].val))  return false;
  if(!IS_INDEX_HEADER_SECOND(idx[1].val)) return false;
  if(!IS_INDEX_BODY_FIRST(idx[2].val))    return false;

  DocumentAddr result;
  result = get_document_addr(idx, 0);
  if(result.sector != d.sector ||  result.offset != d.offset) return false;
  result = get_document_addr(idx, 1);
  if(result.sector != d.sector ||  result.offset != d.offset) return false;
  result = get_document_addr(idx, 2);
  if(result.sector != d.sector ||  result.offset != d.offset) return false;

  PhraseAddr result2;
  result2 = get_phrase_addr(idx, 2);
  if(result2.sector != p1.sector || result2.offset != p1.offset) return false;
  result2 = get_phrase_addr(idx, 5);
  if(result2.sector != p2.sector || result2.offset != p2.offset) return false;


  std::cout << "generating test data...\n";
  shm->init();
  phrase->init();

  char* str = new char[10000*20];
  for(unsigned int i=0; i<10000; i++) {
    InsertPhrase p = {0, 0, {str + i*20}, {0, NULL_PHRASE}};
    sprintf(p.data.value, "%cphrase%05d", ATTR_TYPE_STRING, i);
    phrases.push_back(p);
  }
  phrase->insert(phrases);

  document->init();
  for(unsigned int i=0; i<1000; i++) {
    InsertDocument d = {{10000+i, {i%10, 0, 0, 0}}, {0, NULL_DOCUMENT}};
    d.data.id = 100000+i, d.data.sortkey[0] = i%10;
    docs.push_back(d);
  }
  document->insert(docs);

  init();

  std::cout << "insert and find...\n";
  indexes.clear();

  x.phrases.clear();
  x.doc = docs[5];
  x.phrases.push_back(phrases[0]);
  x.phrases.push_back(phrases[1]);
  indexes.push_back(x);
  insert(indexes);

  result_phrase.clear();
  find(docs[5].addr, result_phrase);
  if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[0].addr) != 0 ||
     phrase_addr_comp(result_phrase[1], phrases[1].addr) != 0) return false;

  std::cout << "remove...\n";
  x.phrases.clear();
  remove(x);
  if(x.phrases.size() != 2 || phrase_addr_comp(x.phrases[0].addr, phrases[0].addr) != 0 || 
     phrase_addr_comp(x.phrases[1].addr, phrases[1].addr) != 0) return false;


  std::cout << "re-insert...\n";
  insert(indexes);
  result_phrase.clear();
  find(docs[5].addr, result_phrase);
  if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[0].addr) != 0 ||
     phrase_addr_comp(result_phrase[1], phrases[1].addr) != 0) return false;


  std::cout << "insert tail...\n";
  indexes.clear();
  x.phrases.clear();
  result_phrase.clear();

  x.doc = docs[10];
  x.phrases.push_back(phrases[3]);
  x.phrases.push_back(phrases[4]);
  indexes.push_back(x);
  insert(indexes);

  find(docs[10].addr, result_phrase);
  if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[3].addr) != 0 ||
     phrase_addr_comp(result_phrase[1], phrases[4].addr) != 0) return false;


  std::cout << "insert head...\n";
  indexes.clear();
  x.phrases.clear();
  result_phrase.clear();

  x.doc = docs[1];
  x.phrases.push_back(phrases[5]);
  x.phrases.push_back(phrases[6]);
  indexes.push_back(x);
  insert(indexes);

  find(docs[1].addr, result_phrase);
  if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[5].addr) != 0 ||
     phrase_addr_comp(result_phrase[1], phrases[6].addr) != 0) return false;


  std::cout << "multi insert...\n";
  indexes.clear();
  for(unsigned int i=11; i<50; i++) {
    x.phrases.clear();
    x.doc = docs[i];
    x.phrases.push_back(phrases[10*i]);
    x.phrases.push_back(phrases[10*i+1]);
    indexes.push_back(x);
  }
  insert(indexes);
  for(unsigned int i=11; i<50; i++) {
    result_phrase.clear();
    find(docs[i].addr, result_phrase);
    if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[10*i].addr) != 0 ||
      phrase_addr_comp(result_phrase[1], phrases[10*i+1].addr) != 0) return false;
  }


  std::cout << "page split...\n";
  for(unsigned int i=100; i<200; i++) {
    indexes.clear();
    x.phrases.clear();
    result_phrase.clear();

    x.doc = docs[i];
    x.phrases.push_back(phrases[10*i]);
    x.phrases.push_back(phrases[10*i+1]);
    indexes.push_back(x);
    insert(indexes);

    find(docs[i].addr, result_phrase);
    if(result_phrase.size() != 2 || phrase_addr_comp(result_phrase[0], phrases[10*i].addr) != 0 ||
      phrase_addr_comp(result_phrase[1], phrases[10*i+1].addr) != 0) return false;
  }

  std::cout << "insert large data...\n";
  indexes.clear();
  x.doc = docs[77];
  x.phrases.clear();
  for(unsigned int i=0; i<1000; i++) {
    x.phrases.push_back(phrases[i]);  
  }
  indexes.push_back(x);
  insert(indexes);

  result_phrase.clear();
  find(docs[77].addr, result_phrase);
  if(result_phrase.size() != 1000) return false;
  for(unsigned int i=0; i<result_phrase.size(); i++) {
    if(phrase_addr_comp(result_phrase[i], phrases[i].addr) != 0) return false;
  }

  std::cout << "remove large data...\n";
  x.phrases.clear();
  remove(x);
  if(x.phrases.size() != 1000) return false;
  for(unsigned int i=0; i<x.phrases.size(); i++) {
    if(phrase_addr_comp(x.phrases[i].addr, phrases[i].addr) != 0) return false;
  }

  result_phrase.clear();
  find(docs[77].addr, result_phrase);
  if(result_phrase.size() != 0) return false;

  delete[] str;

  std::cout << "all test passed!!\n";

  return true;
}


void RegularIndexController::dump() {
  std::cout << "---dump start\n";
  dump(header->root);
  std::cout << "---dump end\n";
}


void RegularIndexController::dump(RegularIndexInfo current) {
  if(current.level > 0) {
    for(unsigned int i=0; i<current.count; i++) {
      load_info(current.pageno, PAGE_READONLY);
      std::cout << "====node " << i << "(pageno " << info[i].pageno << ", count" << info[i].count <<  ") ====\n";
      dump(info[i]);
    }
  } else {
    load_data(current.pageno, PAGE_READONLY);
    dump(data, current.count);
  }
}


void RegularIndexController::dump(RegularIndex* data, unsigned int size) {
    Buffer buf;
    for(unsigned int idx=0; idx<size; idx++) {
      if(IS_INDEX_HEADER_FIRST(data[idx].val)) {
        DocumentAddr d_addr = get_document_addr(data, idx);
        DocumentData d_data = document->find_by_addr(d_addr); 
        std::cout << "  [document id: " << d_data.id << ", sortkey: " << d_data.sortkey[0] << "]";
        std::cout << "(sector:" << d_addr.sector << ", offset:" << d_addr.offset << ")\n";
      } else if(IS_INDEX_BODY_FIRST(data[idx].val)) {
        PhraseAddr p_addr = get_phrase_addr(data, idx);
        PhraseData p_data = phrase->find_data(p_addr, buf);
        if(!p_data.value) std::cout << "    (invalid)  sector: " << p_addr.sector << ", offset: " << p_addr.offset << "\n";
        else if(IS_ATTR_TYPE_STRING(p_data.value[0])) {
          std::cout <<  "    " << p_data.value+1 << "(string) sector: " << p_addr.sector << ", offset: " << p_addr.offset << "\n";
        }
        else {
          std::cout <<  "    " << *((int*)(p_data.value+1)) << "(integer) sector: "  << p_addr.sector << ", offset: " << p_addr.offset << "\n";
        }
      } 
    }
    std::cout << "\n";



}


