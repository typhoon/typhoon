/*****************************************************************
 *  reverse_index_controller.cc
 *    brief: 
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-14 09:38:07 +0900#$
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
#include "reverse_index_controller.h"

ReverseIndexController::ReverseIndexController() {
  data = NULL;
  phrase = NULL;
  document = NULL;
  info = NULL;
  header = NULL;
}

ReverseIndexController::~ReverseIndexController() {
  data = NULL;
  phrase = NULL;
  document = NULL;
  info = NULL;
  header = NULL;
}


bool ReverseIndexController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  base_path = path;
  shm = _shm;

  header = &(shm->get_header()->rev_header);

  data_file.set_file_name(path, DATA_TYPE_REVERSE_INDEX, "dat");
  data_file.set_shared_memory(shm);
  info_file.set_file_name(path, DATA_TYPE_REVERSE_INDEX_INFO, "dat");
  info_file.set_shared_memory(shm);

  data_limit = shm->get_page_size() / sizeof(ReverseIndex);
  info_limit = shm->get_page_size() / sizeof(ReverseIndexInfo);

  return true;
}

bool ReverseIndexController::init(std::string path, SharedMemoryAccess* _shm) { // with setting
  if(!setup(path, _shm)) return false;
  return init();
}


bool ReverseIndexController::init() { // without setting
  if(!clear()) return false;

  if(document) document->finish();
  if(phrase)   phrase->finish();
  if(!init_data()) return false;
  if(!init_info()) return false;
  return true; 
}

bool ReverseIndexController::clear() {
  buf.clear();
  clear_data();
  clear_info();

  data_file.remove_with_suffix();
  info_file.remove_with_suffix();

  return true;
}

bool ReverseIndexController::reset() {
  buf.clear();
  clear_data();
  clear_info();

  return true;
}

bool ReverseIndexController::save() {
  buf.clear();
  if(!save_data()) return false;
  if(!save_info()) return false;
  return true;
}


bool ReverseIndexController::finish() {
  buf.clear();
  clear_data();
  clear_info();

  return true;
}


void ReverseIndexController::set_document_and_phrase(DocumentController* doc, PhraseController* ph) {
  phrase = ph;
  document = doc;
}




/////////////////////////////////////////////////////////////////////////////
//  insert
/////////////////////////////////////////////////////////////////////////////
bool ReverseIndexController::insert(INSERT_REVERSE_INDEX_SET& indexes) {
  if(indexes.size() == 0) return true;

  RANGE r = RANGE(0, indexes.size()-1);
  REVERSE_INDEX_INFO_SET new_info;

  new_info = insert_info(indexes, header->root, r);
  levelup_root_info(new_info);
  save();

  return true;
}


void ReverseIndexController::levelup_root_info(REVERSE_INDEX_INFO_SET& new_info) {
  if(new_info.size() > 1) {
    ReverseIndexInfo first_info  = {0, 0, 0, REVINFO_FLAG_LTERM};
    new_info.insert(new_info.begin(), first_info);

    info_file.add_page(&new_info[0], 0, header->next_info, sizeof(ReverseIndexInfo)*new_info.size());
    (header->root).count = new_info.size();
    (header->root).pageno = header->next_info;
    (header->root).level = new_info[new_info.size()-1].level + 1;
    (header->root).flag = new_info[new_info.size()-1].flag;
    memcpy((header->root).max, new_info[new_info.size()-1].max, sizeof(ReverseIndex)*3);
    (header->next_info)++;
  } else {
    header->root = new_info[0];
  }
}

REVERSE_INDEX_INFO_SET ReverseIndexController::insert_info(INSERT_REVERSE_INDEX_SET& indexes, ReverseIndexInfo current, RANGE r) {
  REVERSE_INDEX_INFO_SET after;
  REVERSE_INDEX_INFO_SET insert;

  MergeData m = {RANGE(0, current.count-1), r};

  load_info(current.pageno, PAGE_READONLY);
  MERGE_SET md = get_insert_info(indexes, m);
  MERGE_SET mi;

  for(unsigned int i=0; i<md.size(); i++) {
    if(md[i].src.first != md[i].src.second) continue;

    REVERSE_INDEX_INFO_SET partial;
    
    save_info();
    load_info(current.pageno, PAGE_READONLY);
    ReverseIndexInfo the_info = info[md[i].src.first];
    try {
      if(the_info.level > 0) {
        partial = insert_info(indexes, the_info, md[i].dst);
      } else {
        partial = insert_data(indexes, the_info, md[i].dst);
      }
    } catch(AppException e) { 
      // If we don't catch this exception, already indexed data becomes inconsistent.
      // Then catch and rollback.
      write_log(LOG_LEVEL_ERROR, e.what(), "");
      partial.push_back(the_info);
    }

    MergeData m = {md[i].src, RANGE(insert.size(), insert.size()+partial.size()-1)};
    mi.push_back(m);
    for(unsigned int j=0; j<partial.size(); j++) { 
      insert.push_back(partial[j]);
    }
  }


  // update B-tree header
  save_info();
  load_info(current.pageno, PAGE_READWRITE);
  unsigned int insert_count = insert.size();
  unsigned int total_count = current.count + insert_count;
  if(total_count <= info_limit) {
    memmove(info+insert_count, info, sizeof(ReverseIndexInfo)*current.count);
    total_count = merge_info(info, info+insert_count, insert, mi, current.count);

    if(is_valid_info(info, total_count) || current.flag == REVINFO_FLAG_RTERM) {
      ReverseIndexInfo new_info = current;
      new_info.count = total_count;
      if(new_info.count > 0) set_max_info(info[new_info.count-1].max, 2, new_info);
      after.push_back(new_info);
    }
  } else {
    ReverseIndexInfo* wbuf = (ReverseIndexInfo*)malloc(sizeof(ReverseIndexInfo)*total_count);
    total_count = merge_info(wbuf, info, insert, mi, current.count);
    if(total_count > info_limit) {
      after = split_info(wbuf, total_count, current);
    } else {
      if(is_valid_info(wbuf, total_count)|| current.flag == REVINFO_FLAG_RTERM) {
        memcpy(info, wbuf, sizeof(ReverseIndexInfo)*total_count);
        ReverseIndexInfo new_info = current;
        new_info.count = total_count;
        if(new_info.count > 0) set_max_info(info[new_info.count-1].max, 2, new_info);
        after.push_back(new_info);
      }
    }
    free(wbuf);
  }


  return after;
}



REVERSE_INDEX_INFO_SET ReverseIndexController::insert_data(INSERT_REVERSE_INDEX_SET& indexes, ReverseIndexInfo current, RANGE r) {
  REVERSE_INDEX_INFO_SET after_insert;

  if(!load_data(current.pageno, PAGE_READWRITE)) return after_insert;

  MergeData init;
  init.src.first = 0, init.src.second = current.count-1;
  init.dst = r;

  MERGE_SET merge = get_insert_data(indexes, init);

  unsigned int insert_count = (r.second-r.first+1)*3;
  unsigned int total_count =  current.count + insert_count;

  if(total_count <= data_limit) {
    memmove(data+insert_count, data, sizeof(ReverseIndex)*current.count);
    total_count = merge_data(data, data+insert_count, indexes, merge);

    if(total_count > 0 || current.flag == REVINFO_FLAG_RTERM) {
      ReverseIndexInfo new_info = current;
      new_info.count = total_count;
      if(new_info.count > 0) set_max_info(data, new_info.count-1, new_info);
      after_insert.push_back(new_info);
    }
  } else {
    ReverseIndex* wbuf = (ReverseIndex*)malloc(sizeof(ReverseIndex)*total_count);
    total_count = merge_data(wbuf, data, indexes, merge);
    if(total_count > data_limit) {
      after_insert = split_data(wbuf, total_count, current);
    } else {
      memcpy(data, wbuf, sizeof(ReverseIndex)*total_count);
      if(total_count > 0 || current.flag == REVINFO_FLAG_RTERM) {
        ReverseIndexInfo new_info = current;
        new_info.count = total_count;
        if(new_info.count > 0) set_max_info(data, new_info.count-1, new_info);
        after_insert.push_back(new_info);
      }
    }
    free(wbuf);
  }

  return after_insert;
}



unsigned int ReverseIndexController::merge_data
(ReverseIndex* wbuf, ReverseIndex* rbuf, INSERT_REVERSE_INDEX_SET& indexes, MERGE_SET& merges) {
  int write_pos = 0, read_pos = 0;

  unsigned int del_h1 = 0xFFFFFFFF;
  unsigned int del_h2 = 0xFFFFFFFF;
  unsigned int del_body = 0xFFFFFFFF;

  int rewrite_from = -1;
  int rewrite_to = -1;

  for(unsigned int i=0; i<merges.size(); i++) {
    int optimize_size = 50 + rand()%20; // avoid worst pattern using randomize

    if(merges[i].dst.first != -1 && merges[i].dst.second != -1) {
      if(rewrite_from == -1) {
        rewrite_from = write_pos < optimize_size ? 0 : write_pos-optimize_size;
        while(rewrite_from<write_pos && !IS_INDEX_HEADER_FIRST(wbuf[rewrite_from].val)) rewrite_from++;
      }

      int ins_pos  = merges[i].dst.first;
      while(ins_pos <= merges[i].dst.second) {
        if(!indexes[ins_pos].delete_flag) {
          wbuf[write_pos++].val = CREATE_REV_INDEX_HEADER_FIRST(indexes[ins_pos].phrase.weight, indexes[ins_pos].phrase.addr.offset);
          wbuf[write_pos++].val = CREATE_REV_INDEX_HEADER_SECOND(indexes[ins_pos].phrase.addr.sector, indexes[ins_pos].doc.addr.sector);
          wbuf[write_pos++].val = CREATE_REV_INDEX_BODY_FIRST(indexes[ins_pos].phrase.pos, indexes[ins_pos].doc.addr.offset);
        } else {
          del_h1 = CREATE_REV_INDEX_HEADER_FIRST(indexes[ins_pos].phrase.weight, indexes[ins_pos].phrase.addr.offset);
          del_h2 = CREATE_REV_INDEX_HEADER_SECOND(indexes[ins_pos].phrase.addr.sector, indexes[ins_pos].doc.addr.sector);
          del_body = CREATE_REV_INDEX_BODY_FIRST(indexes[ins_pos].phrase.pos, indexes[ins_pos].doc.addr.offset);
        }

        ins_pos++;
      }
      if(rewrite_to < write_pos) rewrite_to = write_pos+optimize_size;
    }

    if(merges[i].src.first != -1) {
      unsigned int h1 = del_h1, h2 = del_h2, body = 0xFFFFFFFF;

      while(read_pos<=merges[i].src.second) {
        if(IS_INDEX_HEADER_FIRST(rbuf[read_pos].val))  {
          h1 = rbuf[read_pos].val, body=0xFFFFFFFF;
          if(h1 != del_h1) break;
        } else if(IS_INDEX_HEADER_SECOND(rbuf[read_pos].val)) {
          h2 = rbuf[read_pos].val;
          if(h2 != del_h2) break;
        } else {
          body = rbuf[read_pos].val;
        }

        if(del_h1 == h1 && del_h2 == h2 && del_body == body) {
          read_pos++;
          continue;
        }
        wbuf[write_pos++] = rbuf[read_pos++];
      }
    } 

    if(merges[i].src.first != -1 && rewrite_to != -1 && rewrite_from != -1) {
      while(write_pos < rewrite_to && read_pos <= merges[i].src.second) {
        wbuf[write_pos++] = rbuf[read_pos++];
      }

      if(write_pos >= rewrite_to) {
        while(read_pos <= merges[i].src.second && !IS_INDEX_HEADER_FIRST(rbuf[read_pos].val)) {
          wbuf[write_pos++] = rbuf[read_pos++];
        }

        if(IS_INDEX_BODY(wbuf[write_pos-1].val)) {
          // write_pos = rewrite_data(wbuf, rewrite_from, write_pos);
          rewrite_to = rewrite_from = -1;
        }
      }
    }

    // source data
    if(merges[i].src.first != -1) {
      if(read_pos <= merges[i].src.second) {
        memmove(wbuf+write_pos, rbuf+read_pos, sizeof(ReverseIndex)*(merges[i].src.second-read_pos+1));
        write_pos = write_pos + (merges[i].src.second-read_pos+1);
        read_pos  = merges[i].src.second + 1;
      }
    }
  }

  if(rewrite_to != -1 && rewrite_from != -1) {
    // write_pos = rewrite_data(wbuf, rewrite_from, write_pos);
  }

  write_pos = rewrite_data(wbuf, 0, write_pos);

  return write_pos;
}

int ReverseIndexController::rewrite_data(ReverseIndex* wbuf, int rewrite_from, int rewrite_to) {
  // re-construct
  int rewrite_pos = rewrite_from; 
  unsigned int ofs = 0;
  unsigned int header1 = 0xFFFFFFFF;
  unsigned int header2 = 0xFFFFFFFF;
  unsigned int prev    = 0xFFFFFFFF;
  bool         init_flag = true;


  int i=rewrite_pos;
  while(i<rewrite_to) {
    if(IS_INDEX_HEADER_FIRST(wbuf[i].val)) {
     // if(i+1 < rewrite_to && IS_INDEX_HEADER_SECOND(wbuf[i+1].val)) { 
     if(i+2 < rewrite_to && IS_INDEX_HEADER_SECOND(wbuf[i+1].val) && IS_INDEX_BODY(wbuf[i+2].val)) {
        if(init_flag || wbuf[i].val != header1 || wbuf[i+1].val != header2) {
          header1 = wbuf[i].val;
          header2 = wbuf[i+1].val;
          wbuf[rewrite_pos++] = wbuf[i++];
          wbuf[rewrite_pos++] = wbuf[i++];
          ofs = 0; 
          init_flag = true;
          continue;
        }
      }
    } else if(IS_INDEX_BODY(wbuf[i].val)) {
      if(init_flag || wbuf[i].val != prev) {
        if(ofs > MAX_REVERSE_INDEX_BLOCK) {
          wbuf[rewrite_pos++].val = header1;
          wbuf[rewrite_pos++].val = header2;
          ofs = 0;
        }

        prev = wbuf[i].val;
        wbuf[rewrite_pos++] = wbuf[i++];
        ofs++;
        init_flag = false;
        continue;
      }
    } 
    i++;
  }

  return rewrite_pos;
}


unsigned int ReverseIndexController::merge_info
(ReverseIndexInfo* wbuf, ReverseIndexInfo* rbuf, REVERSE_INDEX_INFO_SET& infos, MERGE_SET& merges, int count)  {
  int read_pos = 0;
  int write_pos = 0;

  for(unsigned int i=0; i<merges.size(); i++) {
    if(merges[i].src.first != merges[i].src.second) continue;

    if(merges[i].src.first != -1) {
      while((int)read_pos <= merges[i].src.first) {
        wbuf[write_pos++] = rbuf[read_pos++];
      }
      if(write_pos > 0) write_pos--;
    }

    if(merges[i].dst.first != -1) {
      int ins_pos = merges[i].dst.first;
      while(ins_pos <= merges[i].dst.second) {
        // std::cout << "insert" << ins_pos << "\n";
        wbuf[write_pos++] = infos[ins_pos++];
      }
    }
  }

  if(read_pos < count) {
    memcpy(wbuf+write_pos, rbuf+read_pos, sizeof(ReverseIndexInfo)*(count-read_pos));
    write_pos = write_pos + count-read_pos;
  }

  return write_pos;
}


MERGE_SET ReverseIndexController::get_insert_info(INSERT_REVERSE_INDEX_SET& indexes, MergeData init) {
  MERGE_SET result;
  MERGE_SET work;

  if(!init_merge_data(result, work, init)) {
    return result;
  }

  while(work.size() > 0) {
    MergeData m = work[work.size()-1];
    work.pop_back();

    if(m.src.first == m.src.second) {
      result.push_back(m);
      continue;
    }

    int srcmid = (m.src.first + m.src.second)/2;
    InsertReverseIndex idx = get_index_data(info[srcmid].max, 2, 2, info[srcmid].flag);
    RANGE dst_range = get_match_data(indexes, idx, m.dst, true);

    MergeData left_set, right_set;
    left_set.src.first  = m.src.first;
    left_set.src.second = srcmid;
    right_set.src.first = srcmid+1;
    right_set.src.second= m.src.second;

    left_set.dst.first  = m.dst.first;
    left_set.dst.second = dst_range.first;
    right_set.dst.first = dst_range.second;
    right_set.dst.second= m.dst.second;

    if(right_set.dst.first <= right_set.dst.second) work.push_back(right_set);
    if(left_set.dst.first  <= left_set.dst.second)  work.push_back(left_set);
  }

  if(g_debug) {
    std::cout << "***\n";
    for(unsigned int i=0; i<result.size(); i++) {
      std::cout << result[i].src.first << "," << result[i].src.second << ":" << result[i].dst.first << "," << result[i].dst.second << "\n";
    }
  }

  return result;
}



MERGE_SET ReverseIndexController::get_insert_data(INSERT_REVERSE_INDEX_SET& indexes, MergeData init) {
  MERGE_SET result;
  MERGE_SET work;

  if(!init_merge_data(result, work, init)) {
    return result;
  }

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
      result.push_back(m);
      continue;
    }

    int srcmid = (m.src.first + m.src.second)/2;
    InsertReverseIndex idx = get_index_data(data, srcmid, init.src.second, REVINFO_FLAG_NONE);
    RANGE dst_range = get_match_data(indexes, idx, m.dst, false);

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
  while(opt_read < result.size()) {
    if(opt_write == 0 || result[opt_read].dst.first != -1) {
      result[opt_write] = result[opt_read];
      opt_write++;
    } else if(result[opt_read].src.first != -1) {
      result[opt_write-1].src.second = result[opt_read].src.second;
    }

    opt_read++;
  }
  result.resize(opt_write);

  if(g_debug) {
    std::cout << "###\n";
    for(unsigned int i=0; i<result.size(); i++) {
      std::cout << result[i].src.first << "," << result[i].src.second << ":" << result[i].dst.first << "," << result[i].dst.second << "\n";
    }
  }

  return result;
}


bool ReverseIndexController::init_merge_data(MERGE_SET& result, MERGE_SET& work, MergeData init) {
  MergeData work_init = init;
  if(work_init.src.first < 0 || work_init.src.second < 0)  { // empty source
    work_init.src.first = work_init.src.second = -1;
    result.push_back(work_init);
    return false;
  }

  work_init.src.second++;  // tail data prepare
  work.push_back(work_init);
  return true;
}


RANGE ReverseIndexController::get_match_data
(INSERT_REVERSE_INDEX_SET& indexes, InsertReverseIndex srcdata, RANGE range, bool include_match) {
  RANGE dst_range;

  if(srcdata.doc.addr.sector == DATA_SECTOR_LEFT) {
    dst_range.first = range.first-1, dst_range.second = range.first;
  } else if(srcdata.doc.addr.sector == DATA_SECTOR_RIGHT) {
    dst_range.first = range.second, dst_range.second = range.second+1;
  } else {
    int l, r;

    l = range.first-1, r = range.second+1;

    while(r-l > 1) {
      int dstmid = (l+r)/2;

      int cmp = reverse_index_comp(indexes[dstmid], srcdata);
      if(cmp<0 || (cmp==0 && include_match)) l = dstmid;
      else                                   r = dstmid;
    }
    dst_range.first  = l;
    dst_range.second = r;
  }

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



///////////////////////////////////////////////
// private methods
///////////////////////////////////////////////
bool ReverseIndexController::save_data() {
  data_file.save_page();
  data = NULL;

  return true;
}

bool ReverseIndexController::save_info() {
  info_file.save_page();
  info = NULL;

  return true;
}


bool ReverseIndexController::clear_data() {
  data_file.clear_page();
  data = NULL;

  return true;
}

bool ReverseIndexController::clear_info() {
  info_file.clear_page();
  info = NULL;

  return true;
}

bool ReverseIndexController::load_data(unsigned int pageno, int mode) {
  data = (ReverseIndex*)data_file.load_page(0, pageno, mode);
  if(!data) return false;

  return true;
}

bool ReverseIndexController::load_info(unsigned int pageno, int mode) {
  info = (ReverseIndexInfo*)info_file.load_page(0, pageno, mode);
  if(!info) return false;

  return true;
}


REVERSE_INDEX_INFO_SET ReverseIndexController::split_data(ReverseIndex* wbuf, unsigned int count, ReverseIndexInfo current) {
  REVERSE_INDEX_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;
  int ofs = 0;
  while(!IS_INDEX_HEADER_FIRST(wbuf[right_pos-ofs].val)) {
    ofs++;
    if(right_pos <= left_pos+ofs) throw AppException(EX_APP_REVINDEX, "failed to split data");
  }
  right_pos = right_pos - ofs;

  // first page
  memcpy(data, wbuf+left_pos, sizeof(ReverseIndex)*(right_pos - left_pos));
  ReverseIndexInfo first_info = {(right_pos-left_pos), current.pageno, 0, REVINFO_FLAG_NONE};
  set_max_info(wbuf, right_pos-1, first_info);
  after_info.push_back(first_info);


  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos + ofs;
    ofs = 0;

    if(right_pos >= count) right_pos = count;
    else {
      while(!IS_INDEX_HEADER_FIRST(wbuf[right_pos-ofs].val)) {
        ofs++;
        if(right_pos <= left_pos+ofs) throw AppException(EX_APP_REVINDEX, "failed to split data");
      }
      right_pos = right_pos - ofs;
    }

    data_file.add_page(wbuf+left_pos, 0, header->next_data, sizeof(ReverseIndex)*(right_pos-left_pos));
    ReverseIndexInfo add_info = {(right_pos-left_pos), header->next_data, 0, REVINFO_FLAG_NONE};
    set_max_info(wbuf, right_pos-1, add_info);
    after_info.push_back(add_info);
    header->next_data++;
  }

  if(after_info.size() > 0 && current.flag == REVINFO_FLAG_RTERM) {
    after_info[after_info.size()-1].flag = REVINFO_FLAG_RTERM;
  }

  return after_info;
}


REVERSE_INDEX_INFO_SET ReverseIndexController::split_info(ReverseIndexInfo* wbuf, unsigned int count, ReverseIndexInfo current) {
  REVERSE_INDEX_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-MAX_REVERSE_INDEX_BLOCK-2) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(info, wbuf+left_pos, sizeof(ReverseIndexInfo)*(right_pos - left_pos));
  ReverseIndexInfo first_info = {(right_pos-left_pos), current.pageno, 1, REVINFO_FLAG_NONE};
  set_max_info(wbuf[right_pos-1].max, 2, first_info);
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    info_file.add_page(wbuf+left_pos, 0, header->next_info, sizeof(ReverseIndexInfo)*(right_pos-left_pos));
    ReverseIndexInfo add_info = {(right_pos-left_pos), header->next_info, 1, REVINFO_FLAG_NONE};
    set_max_info(wbuf[right_pos-1].max, 2, add_info);
    after_info.push_back(add_info);
    header->next_info++;
  }
 
  if(after_info.size() > 0 && current.flag == REVINFO_FLAG_RTERM) {
    after_info[after_info.size()-1].flag = REVINFO_FLAG_RTERM;
  }

  return after_info;
}



bool ReverseIndexController::init_data() {
  data_file.add_page(NULL, 0, 0);
  header->next_data = 1;

  return true;
}



bool ReverseIndexController::init_info() {
  REVERSE_INDEX_INFO_SET init;
  ReverseIndexInfo first_info  = {0, 0, 0, REVINFO_FLAG_LTERM};
  ReverseIndexInfo second_info = {0, 0, 0, REVINFO_FLAG_RTERM};
  init.push_back(first_info);
  init.push_back(second_info);
  info_file.add_page(&init[0], 0, 0, sizeof(ReverseIndexInfo)*sizeof(init));
  header->next_info = 1;
  
  (header->root).count  = 2;
  (header->root).pageno = 0;
  (header->root).level  = 1;
  (header->root).flag = REVINFO_FLAG_RTERM;

  return true;
}


unsigned int ReverseIndexController::get_phrase_pos(ReverseIndex* idx, unsigned int pos) {
  if(!IS_INDEX_BODY(idx[pos].val)) return 0;
  return REV_INDEX_PHRASE_POS(idx[pos].val);
}

PhraseAddr ReverseIndexController::get_phrase_addr(ReverseIndex* idx, unsigned int pos) {
  unsigned int ofs = 0;
  PhraseAddr result = {0, NULL_PHRASE};

  while(!IS_INDEX_HEADER_FIRST(idx[pos-ofs].val)) {
    if(ofs > pos || ofs > MAX_REVERSE_INDEX_BLOCK+2) {
      throw AppException(EX_APP_REVINDEX, "failed to get phrase addr");
    }
    ofs++;
  }

  result.offset = REV_INDEX_PHRASE_OFFSET(idx[pos-ofs].val);
  if(IS_INDEX_HEADER_SECOND(idx[pos-ofs+1].val)) {
    result.sector = REV_INDEX_PHRASE_SECTOR(idx[pos-ofs+1].val);
  }

  return result;
}


DocumentAddr ReverseIndexController::get_document_addr(ReverseIndex* idx, unsigned int pos) {
  unsigned int ofs = 0;
  DocumentAddr result = {0, NULL_PHRASE};

  if(!IS_INDEX_BODY(idx[pos].val)) return result;

  while(!IS_INDEX_HEADER_FIRST(idx[pos-ofs].val)) {
    if(ofs > pos || ofs > MAX_REVERSE_INDEX_BLOCK+2) {
      throw AppException(EX_APP_REVINDEX, "failed to get document addr");
    }
    ofs++;
  } 

  return get_document_addr(idx, pos, pos-ofs);
}


DocumentAddr ReverseIndexController::get_document_addr(ReverseIndex* idx, unsigned int doc_pos, unsigned int phrase_pos) {
  DocumentAddr result = {0, NULL_DOCUMENT};

  if(!IS_INDEX_BODY(idx[doc_pos].val) || !IS_INDEX_HEADER_FIRST(idx[phrase_pos].val)) {
    throw AppException(EX_APP_REVINDEX, "failed to get document addr");
  }

  result.offset = REV_INDEX_DOCUMENT_OFFSET(idx[doc_pos].val);
  if(IS_INDEX_HEADER_SECOND(idx[phrase_pos+1].val)) {
    result.sector = REV_INDEX_DOCUMENT_SECTOR(idx[phrase_pos+1].val);
  }

  return result;
}


InsertReverseIndex ReverseIndexController::get_index_data(ReverseIndex* idx, unsigned int pos, unsigned int limit, char flag) {
  InsertReverseIndex i;
  i.phrase.weight = i.phrase.pos = 0;
  i.phrase.addr.offset = NULL_PHRASE;
  i.doc.addr.offset = NULL_DOCUMENT;
  i.delete_flag = false;

  if(flag == REVINFO_FLAG_LTERM) {
    i.phrase.addr.sector = i.doc.addr.sector = DATA_SECTOR_LEFT; 
  } else if(flag == REVINFO_FLAG_RTERM) {
    i.phrase.addr.sector = i.doc.addr.sector = DATA_SECTOR_RIGHT; 
  } else if(flag == REVINFO_FLAG_EMPTY) {
    i.doc.addr.sector = i.phrase.addr.sector = 0;
  } else {
    i.phrase.weight = 0;
    i.phrase.addr = get_phrase_addr(idx, pos);
    i.phrase.data = phrase->find_data(i.phrase.addr, buf);

    if(IS_INDEX_BODY(idx[pos].val)) {
      i.phrase.pos = REV_INDEX_PHRASE_POS(idx[pos].val);
      i.doc.addr = get_document_addr(idx, pos);
      i.doc.data = document->find_by_addr(i.doc.addr);
    } else if(pos+1 <= limit && IS_INDEX_BODY(idx[pos+1].val)) {
      i.phrase.pos = REV_INDEX_PHRASE_POS(idx[pos+1].val);
      i.doc.addr = get_document_addr(idx, pos+1);
      i.doc.data = document->find_by_addr(i.doc.addr);
    } else if(pos+2 <= limit && IS_INDEX_BODY(idx[pos+2].val)) {
      i.phrase.pos = REV_INDEX_PHRASE_POS(idx[pos+2].val);
      i.doc.addr = get_document_addr(idx, pos+2);
      i.doc.data = document->find_by_addr(i.doc.addr);
    }
  }

  return i;
}

void ReverseIndexController::set_max_info(ReverseIndex* idx, unsigned int pos, ReverseIndexInfo& i) {
  if(i.flag != REVINFO_FLAG_NONE) return;
  if(IS_INDEX_BODY(idx[pos].val)) {
    i.max[2] = idx[pos];
  } else {
    throw AppException(EX_APP_REVINDEX, "hoge");
  } 

  unsigned int ofs = 0;
  while(!IS_INDEX_HEADER_FIRST(idx[pos-ofs].val)) {
    ofs ++;
    if(ofs > pos) throw AppException(EX_APP_REVINDEX, "hoge");
  } 

  i.max[0] = idx[pos-ofs];
  i.max[1] = idx[pos-ofs+1];
}


// standard search
unsigned int ReverseIndexController::find(const void* search_data, ID_SET& result) {
  unsigned int total_hit_count = 0;

  DocumentAddr next_addr = document->get_document_data()->get_next_addr();
  for(unsigned short s=0; s<=next_addr.sector; s++) {
    SEARCH_RESULT_RANGE_SET sr;
    find_range(search_data, sr, s);
    for(unsigned int i=0; i<sr.size(); i++) {
      total_hit_count = total_hit_count + search_range_to_idset(sr[i], result);
    }
  }
  return total_hit_count;
}

unsigned int ReverseIndexController::find_range(const void* search_data, SEARCH_RESULT_RANGE_SET& sr, unsigned short sector) {
  return find_range_common(search_data, search_data, sr, header->root, sector);
}


// prefix search(string only)
unsigned int ReverseIndexController::find_prefix(const char* prefix, ID_SET& result) {
  unsigned int total_hit_count = 0;

  unsigned short the_sector = document->get_document_data()->get_next_addr().sector;
  for(unsigned short s=0; s<=the_sector; s++) {
    SEARCH_RESULT_RANGE_SET sr;
    find_prefix_range(prefix, sr, s);
    for(unsigned int i=0; i<sr.size(); i++) {
      total_hit_count = total_hit_count + search_range_to_idset(sr[i], result);
    }
  }

  return total_hit_count;
}


unsigned int ReverseIndexController::find_prefix_range(const char* prefix, SEARCH_RESULT_RANGE_SET& sr, unsigned short sector) {
  unsigned int len = strlen(prefix+1);
  char max_prefix_data[sizeof(char)+len+4];
  max_prefix_data[0] = prefix[0];
  memcpy(max_prefix_data, prefix, len+1);
  max_prefix_data[len+1] = 0xFF;
  max_prefix_data[len+2] = 0xFF;
  max_prefix_data[len+3] = 0xFF;
  max_prefix_data[len+4] = 0x00;

  return find_range_common(prefix, max_prefix_data, sr, header->root, sector);
}

// range search
unsigned int ReverseIndexController::find_between(const void* min, const void* max, ID_SET& result) {
  unsigned int total_hit_count = 0;

  unsigned short the_sector = document->get_document_data()->get_next_addr().sector;
  for(unsigned short s=0; s<=the_sector; s++) {
    SEARCH_RESULT_RANGE_SET sr;
    find_between_range(min, max, sr, s);
    for(unsigned int i=0; i<sr.size(); i++) {
      total_hit_count = total_hit_count + search_range_to_idset(sr[i], result);
    }
  }

  return total_hit_count;
}


unsigned int ReverseIndexController::find_between_range(const void* min, const void* max, SEARCH_RESULT_RANGE_SET& sr, unsigned short sector) {
  return find_range_common(min, max, sr, header->root, sector);
}



unsigned int ReverseIndexController::find_range_common
(const void* str_from, const void* str_to, SEARCH_RESULT_RANGE_SET& sr, ReverseIndexInfo page_info, unsigned short sector) {
  int l, r;
  PhraseData pmin = {(char*)str_from};
  PhraseData pmax = {(char*)str_to};

  load_info(page_info.pageno, PAGE_READONLY);
  l = -1, r = page_info.count-1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    if(info[dstmid].flag == REVINFO_FLAG_LTERM) {
      l = dstmid;
    }
    else if(info[dstmid].flag == REVINFO_FLAG_RTERM) {
      r = dstmid;
    }
    else {
      InsertReverseIndex i = get_index_data(info[dstmid].max, 2, 2, info[dstmid].flag);
      int cmp = (i.doc.addr.sector - sector);
      if(cmp == 0) cmp = phrase_data_comp(i.phrase.data, pmin);

      if(cmp < 0) l = dstmid;
      else        r = dstmid;
    }
  }
  int l_limit = r;

  l = -1, r = page_info.count-1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    if(info[dstmid].flag == REVINFO_FLAG_LTERM) {
      l = dstmid;
    }
    else if(info[dstmid].flag == REVINFO_FLAG_RTERM) {
      r = dstmid;
    }
    else {
      InsertReverseIndex i = get_index_data(info[dstmid].max, 2, 2, info[dstmid].flag);
      int cmp = (i.doc.addr.sector - sector);
      if(cmp == 0) cmp = phrase_data_comp(i.phrase.data, pmax);

      if(cmp <= 0)  l = dstmid;
      else          r = dstmid;
    }
  }
  int r_limit = r; 

  for(int i=l_limit; i<=r_limit; i++) {
    load_info(page_info.pageno, PAGE_READONLY);
    ReverseIndexInfo the_info = info[i];
    if(info[i].level > 0) {
      find_range_common(str_from, str_to, sr, the_info, sector);
    } else {
      SearchResultRange s = {the_info.pageno, 0, the_info.count-1};
      if(i==l_limit) s.left_offset = find_left(str_from, the_info, sector);
      if(i==r_limit) s.right_offset = find_right(str_to, the_info, sector);

      sr.push_back(s);
    }
  }

  return sr.size();
}


int ReverseIndexController::find_left(const void* str_from, ReverseIndexInfo page_info, unsigned short sector) {
  int l, r;
  PhraseData pmin = {(char*)str_from};
  load_data(page_info.pageno, PAGE_READONLY);
  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    InsertReverseIndex i = get_index_data(data, dstmid, page_info.count, REVINFO_FLAG_NONE);
    int cmp = (i.doc.addr.sector - sector);
    if(cmp == 0) cmp = phrase_data_comp(i.phrase.data, pmin);

    if(cmp < 0) l = dstmid;
    else        r = dstmid;
  }
  return r;
}

int ReverseIndexController::find_right(const void* str_to, ReverseIndexInfo page_info, unsigned short sector) {
  int l, r;
  PhraseData pmax = {(char*)str_to};

  load_data(page_info.pageno, PAGE_READONLY);
  l = -1, r = page_info.count;
  while(r-l > 1) {
    int dstmid = (l+r)/2;

    InsertReverseIndex i = get_index_data(data, dstmid, page_info.count, REVINFO_FLAG_NONE);
    int cmp = (i.doc.addr.sector - sector);
    if(cmp == 0) cmp = phrase_data_comp(i.phrase.data, pmax);

    if(cmp <= 0)  l = dstmid;
    else          r = dstmid;
  }

  return l;
}


unsigned int ReverseIndexController::search_range_to_idset(SearchResultRange& sr, ID_SET& result) {
  int count = 0;

  if(sr.pageno < 0)  return 0;
  load_data(sr.pageno, PAGE_READONLY);

  for(int i=sr.left_offset; i<=sr.right_offset; i++) {
    if(IS_INDEX_BODY(data[i].val)) {
      DocumentAddr a = get_document_addr(data, i);
      DocumentData d = document->find_by_addr(a);
      result.push_back(d.id);
      count++;
    }
  }
  
  return count;
}


int ReverseIndexController :: find_hit_data_all(SEARCH_HIT_DATA_SET& hits, SEARCH_RESULT_RANGE_SET& sr_set, ATTR_TYPE_SET& order) {
  int cnt = 0;
  for(unsigned int idx=0; idx<sr_set.size(); idx++) {
    cnt = cnt + find_hit_data_partial(hits, sr_set, idx, order);
  }

  return cnt;
}


int ReverseIndexController :: find_hit_data_partial(SEARCH_HIT_DATA_SET& hits, SEARCH_RESULT_RANGE_SET& sr_set, unsigned int idx, ATTR_TYPE_SET& order) {
  if(idx >= sr_set.size()) return 0;

  load_data(sr_set[idx].pageno, PAGE_READONLY);

  int cnt = 0;
  for(int i=sr_set[idx].left_offset; i<=sr_set[idx].right_offset; i++) {
    if(IS_INDEX_BODY(data[i].val)) {
      DocumentAddr a = get_document_addr(data, i);
      DocumentData d = document->find_by_addr(a);
      SearchHitData h = {false, d.id, {0, 0, 0, 0}, REV_INDEX_PHRASE_POS(data[i].val)};
      if(order.size() == 0) {
        memcpy(&h.sortkey[0], &d.sortkey[0], sizeof(int)*SORT_KEY_COUNT);
      } else {
        int current_bit = 0;
        unsigned int k, shift_bit, base;
        for(unsigned int i=0; i<order.size(); i++) {
          // align to bit head
          k = order[i].bit_from >> 5;
          shift_bit = order[i].bit_from & 0x1F;
          base = d.sortkey[k] << shift_bit;
          if(shift_bit + order[i].bit_len > 32) {
            base = (base | d.sortkey[k+1] >> (order[i].bit_len-shift_bit));
          }
          if(order[i].bit_reverse_flag) base = base ^ 0xFFFFFFFF;
          base = base & (0xFFFFFFFF << (32-order[i].bit_len));
          

          // map to hit data
          k = current_bit >> 5;
          shift_bit    =  current_bit & 0x1F;
          unsigned int first_mask   =  base >> shift_bit;
          h.sortkey[k] |= first_mask;
          if(shift_bit + order[i].bit_len > 32) {
            unsigned int second_mask  =  base << (32-shift_bit);
            if(second_mask != 0) h.sortkey[k+1] |= second_mask;
          }
          current_bit += order[i].bit_len;
        }
      }
 
      hits.push_back(h); 
      cnt++;
    }
  }
  
  return cnt;
}


bool ReverseIndexController::is_valid_info(ReverseIndexInfo* info, unsigned int total_count) {
    for(unsigned int i=0; i<total_count; i++) {
      if(info[i].flag == REVINFO_FLAG_NONE) return true;
    }
   return false;
}



////////////////////////////////////////////////////////////////
//    for debug
////////////////////////////////////////////////////////////////
void ReverseIndexController::dump() {
  std::cout << "---dump start\n";
  dump(header->root);
  std::cout << "---dump end\n";
}


void ReverseIndexController::dump(ReverseIndexInfo current) {
  if(current.level > 0) {
    for(unsigned int i=0; i<current.count; i++) {
      load_info(current.pageno, PAGE_READONLY);
      std::cout << "====node " << i << "====\n";
      std::cout << " (pageno " << info[i].pageno << ", count" << info[i].count <<  ", level" << (unsigned int)info[i].level << ") \n";

      if(info[i].flag == REVINFO_FLAG_NONE) {
        PhraseAddr p_addr = get_phrase_addr(info[i].max, 2);
        PhraseData p_data = phrase->find_data(p_addr, buf);
        unsigned int p_pos = get_phrase_pos(info[i].max, 2);
        DocumentData d_data = document->find_by_addr(get_document_addr(info[i].max, 2));
        if(p_data.value && IS_ATTR_TYPE_STRING(p_data.value[0])) {
          std::cout << " (max phrase: " << p_data.value+1 << "@" << p_pos << ", max doc: " << d_data.id  << ") \n";
        }
      } else if(info[i].flag == REVINFO_FLAG_LTERM) {
        std::cout << "  (LEFT TERMINATOR)\n";
      } else if(info[i].flag == REVINFO_FLAG_RTERM) {
        std::cout << "  (RIGHT_TERMINATOR)\n";
      }
      dump(info[i]);
    }
  } else {
    load_data(current.pageno, PAGE_READONLY);
    dump(data, current.count);
  }
}


void ReverseIndexController::dump(ReverseIndex* d, unsigned int size) {
  for(unsigned int idx=0; idx<size; idx++) {
    if(IS_INDEX_HEADER_FIRST(d[idx].val)) {
      PhraseAddr p_addr = get_phrase_addr(d, idx);
      PhraseData p_data = phrase->find_data(p_addr, buf);
      std::cout << "[" << (unsigned int)p_data.value[0] << " ";
      if(IS_ATTR_TYPE_STRING(p_data.value[0])) {
        std::cout << p_data.value+1 << "(sect:" << p_addr.sector << " offset:" << p_addr.offset << ")";
      } else {
        std::cout << *((int*)(p_data.value+1)) << "(sect:" << p_addr.sector << " offset:" << p_addr.offset << ")";
      }
      std::cout << "]";
    }
    else if(IS_INDEX_HEADER_SECOND(d[idx].val)) {
      std::cout << "\n";
    }
    else if(IS_INDEX_BODY(d[idx].val)) {
      DocumentAddr d_addr = get_document_addr(d, idx);
      DocumentData d_data = document->find_by_addr(d_addr);
      std::cout << "    [" << d_data.id << "(pos:" << REV_INDEX_PHRASE_POS(d[idx].val) << " rank:" << d_data.sortkey[0] << 
                   " sect:" << d_addr.sector << " offset:" << d_addr.offset << ")]\n";
    }
  }
  std::cout << "\n";
}



bool ReverseIndexController::test() {
  INSERT_DOCUMENT_SET docs;
  INSERT_PHRASE_SET   phrases;
  INSERT_REVERSE_INDEX_SET inserts;
  ID_SET res;

  data_limit = 100;
  info_limit = 8;

  std::cout << "macro test...\n";
  REVERSE_INDEX_SET indexes;
  ReverseIndex i;
  i.val = CREATE_REV_INDEX_HEADER_FIRST(0, 10);
  indexes.push_back(i);
  i.val = CREATE_REV_INDEX_HEADER_SECOND(1, 2);
  indexes.push_back(i);
  i.val = CREATE_REV_INDEX_BODY_FIRST(0, 20);
  indexes.push_back(i);
  i.val = CREATE_REV_INDEX_BODY_FIRST(1, 30);
  indexes.push_back(i);
  i.val = CREATE_REV_INDEX_BODY_FIRST(0, 40);
  indexes.push_back(i);
 
  if(!IS_INDEX_HEADER(indexes[0].val)) return false;
  if(!IS_INDEX_HEADER(indexes[1].val)) return false;
  if(!IS_INDEX_BODY(indexes[2].val)) return false;

 
  DocumentAddr doc_addr;
  PhraseAddr   phrase_addr;
  doc_addr = get_document_addr(&indexes[0], 0); 
  if(doc_addr.offset != NULL_DOCUMENT) return false;
  doc_addr = get_document_addr(&indexes[0], 3); 
  if(doc_addr.offset != 30 || doc_addr.sector != 2) return false;

  phrase_addr = get_phrase_addr(&indexes[0], 0);
  if(phrase_addr.offset != 10 || phrase_addr.sector != 1) return false;

  phrase_addr = get_phrase_addr(&indexes[0], 3);
  if(phrase_addr.offset != 10 || phrase_addr.sector != 1) return false;
  

  std::cout << "initialize...\n";
  if(!init()) return false;

  std::cout << "generating phrase...\n";
  phrase->init();
  char* str = new char[100000*20];
  for(unsigned int i=0; i<100000; i++) {
    InsertPhrase p = {100, 2, {str + i*20}, {0, NULL_PHRASE}};
    sprintf(p.data.value, "%cphrase%05d", ATTR_TYPE_STRING, i);
    phrases.push_back(p);
  }
  phrase->insert(phrases);
  phrase->save();

  std::cout << "generating document...\n";
  document->init();
  for(unsigned int i=0; i<10000; i++) {
    InsertDocument d = {{100000+i, {i%10, 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d);
  }
  document->insert(docs);
  document->save();


  InsertReverseIndex ins;
  ins.delete_flag = false;

  std::cout << "data sort test...\n";
  docs[0].addr.sector = 2;
  inserts.clear();
  ins.doc = docs[0];
  ins.phrase = phrases[0];
  inserts.push_back(ins);
  ins.doc = docs[2];
  ins.phrase = phrases[1];
  inserts.push_back(ins);
  ins.doc = docs[2];
  ins.phrase = phrases[2];
  inserts.push_back(ins);
  ins.doc = docs[1];
  ins.phrase = phrases[1];
  inserts.push_back(ins);
  ins.doc = docs[1];
  ins.phrase = phrases[2];
  inserts.push_back(ins);

  // sorted by phrase-asc, sector-asc, rank-asc, id-desc
  INSERT_REVERSE_INDEX_SET sort_inserts(inserts.size());
  copy(inserts.begin(), inserts.end(), sort_inserts.begin());
  sort(sort_inserts.begin(), sort_inserts.end(), InsertReverseIndexComp());
  if(reverse_index_comp(sort_inserts[0], inserts[3]) != 0) return false;
  if(reverse_index_comp(sort_inserts[1], inserts[1]) != 0) return false;
  if(reverse_index_comp(sort_inserts[2], inserts[4]) != 0) return false;
  if(reverse_index_comp(sort_inserts[3], inserts[2]) != 0) return false;
  if(reverse_index_comp(sort_inserts[4], inserts[0]) != 0) return false;
  docs[0].addr.sector = 0;


  std::cout << "insert and remove test...\n";
for(unsigned int dcnt = 1; dcnt < 50; dcnt++) {
for(unsigned int pcnt = 1; pcnt < 30; pcnt++) {
  std::cout << dcnt << "," << pcnt << "\n";
  g_debug = false;

  ins.delete_flag = false;
  for(unsigned int i=0; i<dcnt; i++) {
    ins.doc = docs[i];

    inserts.clear();
    for(unsigned int j=0; j<pcnt; j++) {
      ins.phrase =phrases[j];
      ins.phrase.pos = j / (1+pcnt/128);
      inserts.push_back(ins);
    }
    sort(inserts.begin(), inserts.end(), InsertReverseIndexComp());
    insert(inserts);
  }

  for(unsigned int i=0; i<pcnt; i++) {
    res.clear();
    find(phrases[i].data.value, res);
    if(res.size() != dcnt) {
      return false;
    }
  }

  ins.delete_flag = true;
  for(unsigned int i=0; i<dcnt; i++) {
    ins.doc = docs[i];

    inserts.clear();
    for(unsigned int j=0; j<pcnt; j++) {
      ins.phrase = phrases[j];
      ins.phrase.pos = j / (1+pcnt/128);
      inserts.push_back(ins);
    }
    sort(inserts.begin(), inserts.end(), InsertReverseIndexComp());
    insert(inserts);
  }


  for(unsigned int i=0; i<pcnt; i++) {
    res.clear();
    find(phrases[i].data.value, res);
    if(res.size() != 0) {
      dump();
      return false;
    }
  }
}
}

  std::cout << "invalid remove test...\n";
  inserts.clear();
  ins.phrase = phrases[0];
  ins.doc = docs[0];
  inserts.push_back(ins);
  insert(inserts);

  std::cout << "another data insert test...\n";
  // shm->init();
  // init();
  ins.delete_flag = false;
 
  unsigned int cnt = 100; 
  for(unsigned int i=0; i<cnt; i++) {
    ins.phrase = phrases[i];

    for(unsigned int j=10; j<11; j++) {
      inserts.clear();
      ins.doc = docs[j];
      inserts.push_back(ins);
      insert(inserts);
    }
  }

  for(unsigned int i=0; i<cnt; i++) {
    res.clear();
    find(phrases[i].data.value, res);
    if(res.size() != 1 || res[0] != docs[10].data.id) return false;
  }
  
  std::cout << "same data insert test...\n";
  for(unsigned int i=0; i<cnt; i++) {
    ins.phrase = phrases[i];

    for(unsigned int j=10; j<11; j++) {
      inserts.clear();
      ins.doc = docs[j];
      inserts.push_back(ins);
      insert(inserts);
    }
  }

  for(unsigned int i=0; i<cnt; i++) {
    res.clear();
    find(phrases[i].data.value, res);
    if(res.size() != 1) return false;
  }

  std::cout << "large data insert at one time test...\n";
  inserts.clear();
  for(unsigned int i=0; i<1000; i++) {
    ins.doc = docs[i];
    for(unsigned int j=0; j<5; j++) {
      ins.phrase = phrases[j];
      inserts.push_back(ins);
    }
  }
  sort(inserts.begin(), inserts.end(), InsertReverseIndexComp());
  insert(inserts); 

  for(unsigned int j=0; j<5; j++) {
    res.clear();
    find(phrases[j].data.value, res);
    if(res.size() != 1000) return false;
  }


  std::cout << "empty insert test...\n";
  inserts.clear();
  insert(inserts);

  for(unsigned int j=0; j<5; j++) {
    res.clear();
    find(phrases[j].data.value, res);
    if(res.size() != 1000) return false;
  }


  return true;
}

