/********************************************************************************
 *   document_controller.cc
 *     brief: Document data consists of three parts, document base data, 
 *            Document base data is composed of original DOCUMENT_ID and rank placed on fixed address.
 *            (We can expand this data format.)
 *            that is sorted by base 
 *            Document data is needed fixed address access and original ID based access,
 *            then we split base data and address data.
 *      Note: If we use original ID instead of fixed address, we needs O(logN) binary-search
 *            when calling document data from index data. Call by address is O(1).
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 11:46:34 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *******************************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include "document_controller.h"

/////////////////////////////////////////////
// constructor & destructor
/////////////////////////////////////////////
DocumentController::DocumentController() {
  document_data = NULL;
  data = NULL;
  info = NULL;
  shm = NULL;
}

DocumentController::~DocumentController() {
  document_data = NULL;
  data = NULL;
  info = NULL;
  shm = NULL;
}



/////////////////////////////////////////////
// public methods
/////////////////////////////////////////////
bool DocumentController::init(std::string path, SharedMemoryAccess* _shm) { // with settings
  if(!setup(path, _shm)) return false;
  return init();
}

bool DocumentController::init() { // without settings
  if(!clear()) return false;
  if(!init_data()) return false;
  if(!init_info()) return false;

  return true;
}

bool DocumentController::clear() {
  clear_data();
  clear_info();
  if(document_data) document_data->clear();

  data_file.remove_with_suffix();
  info_file.remove_with_suffix();

  return true;
}

bool DocumentController::reset() {
  clear_data();
  clear_info();
  if(document_data && !document_data->reset()) return false;

  return true;
}

bool DocumentController::save() {
  if(document_data && !document_data->save()) return false;

  if(!save_data()) return false;
  if(!save_info()) return false;
  return true;
}

bool DocumentController::finish() {
  clear_data();
  clear_info();
  if(document_data) document_data->finish();

  return true;  
}



bool DocumentController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  base_path = path;
  shm = _shm;
  header = &(shm->get_header()->d_header);

  data_file.set_file_name(path, DATA_TYPE_DOCUMENT_ADDR, "dat");
  data_file.set_shared_memory(shm);
  data_limit = shm->get_page_size() / sizeof(DocumentAddr);

  info_file.set_file_name(path, DATA_TYPE_DOCUMENT_INFO, "dat");
  info_file.set_shared_memory(shm);
  info_limit = shm->get_page_size() / sizeof(DocumentInfo);

  return true;
}



/////////////////////////////////////////////////////////////////
// find
/////////////////////////////////////////////////////////////////
DocumentData DocumentController::find(unsigned int doc_id) {
  DocumentData emptydata = {NULL_DOCUMENT, {0, 0, 0, 0}};
  DocumentInfo info = find_info(doc_id);
  int l = -1, r = info.count;
  int mid;

  if(info.pageno < 0 || !load_data(info.pageno, PAGE_READONLY)) goto find_error;
 
  while(r-l > 1) {
    mid = (l+r)/2;

    DocumentData mid_data = find_by_addr(data[mid]);
    if(mid_data.id == NULL_DOCUMENT) goto find_error;

    if(mid_data.id < doc_id)       l=mid;
    else if(mid_data.id == doc_id) {
      return mid_data;
    }
    else                           r=mid;
  }

find_error:
  return emptydata;
}

DocumentData DocumentController::find_by_id(unsigned int id) {
  return find(id);
}

DocumentData DocumentController::find_by_addr(DocumentAddr addr) {
  return document_data->find(addr);
}

DocumentAddr DocumentController::find_addr(unsigned int doc_id) {
  DocumentAddr ret = {0, NULL_DOCUMENT};
  DocumentInfo info = find_info(doc_id);

  if(info.pageno < 0 || !load_data(info.pageno, PAGE_READONLY)) return ret;
 
  int l = -1, r=info.count;
  int mid;
  while(r-l > 1) {
    mid = (l+r)/2;

    DocumentData mid_data = find_by_addr(data[mid]);
    if(mid_data.id == NULL_DOCUMENT) {
      std::cout << "invalid document specified\n";
      return ret;
    }

    if(mid_data.id < doc_id)  l=mid;
    else if(mid_data.id == doc_id) return data[mid];
    else                          r=mid;
  }

  return ret;
}

DocumentAddr DocumentController::find_addr_by_id(unsigned int doc_id) {
  return find_addr(doc_id);
}




/////////////////////////////////////////////////////////////////
// insert
/////////////////////////////////////////////////////////////////
DocumentAddr DocumentController::insert(DocumentData _data) {
  INSERT_DOCUMENT_SET docs;
  InsertDocument d = {{}, {0, NULL_DOCUMENT}};
  d.data = _data;
  docs.push_back(d);
  insert(docs);
  return docs[0].addr;
}

bool DocumentController::insert(INSERT_DOCUMENT_SET& docs) {
  if(docs.size() == 0) return true;

  RANGE r = RANGE(0, docs.size()-1);

  DOCUMENT_INFO_SET new_info = insert_info(docs, header->root, r); 
  if(new_info.size() > 1) {
    info_file.add_page(&new_info[0], 0, header->next_info, sizeof(DocumentInfo)*new_info.size());
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


DOCUMENT_INFO_SET DocumentController::insert_info(INSERT_DOCUMENT_SET& docs, DocumentInfo current, RANGE r) {
  DOCUMENT_INFO_SET after;
  DOCUMENT_INFO_SET insert;

  MergeData m = {RANGE(0, current.count-1), r}; 

  load_info(current.pageno, PAGE_READONLY);
  MERGE_SET md = get_insert_info(docs, m);
  MERGE_SET mi;
  for(unsigned int i=0; i<md.size(); i++) {
    if(md[i].src.first != md[i].src.second) continue;

    DOCUMENT_INFO_SET partial;
    MergeData m = {md[i].src};

    save_info();
    load_info(current.pageno, PAGE_READONLY);
    DocumentInfo the_info = info[md[i].src.first];

    try {
      if(the_info.level > 0) {
        partial = insert_info(docs, the_info, md[i].dst);
      } else {
        partial = insert_data(docs, the_info, md[i].dst);
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
    memmove(info+insert_count, info, sizeof(DocumentInfo)*current.count);
    total_count = merge_info(info, info+insert_count, insert, mi, current.count);

    DocumentInfo new_info = current;
    new_info.count = total_count;
    after.push_back(new_info); 
  } else {
    DocumentInfo* wbuf = (DocumentInfo*)malloc(sizeof(DocumentInfo)*total_count);
    total_count = merge_info(wbuf, info, insert, mi, current.count);
    if(total_count > info_limit) {
      after = split_info(wbuf, total_count, current.pageno);
    } else {
      memcpy(info, wbuf, sizeof(DocumentInfo)*total_count);
      DocumentInfo new_info = current;
      new_info.count = total_count;
      after.push_back(new_info);
    } 
    free(wbuf);
  }

  return after; 
}

DOCUMENT_INFO_SET DocumentController::insert_data(INSERT_DOCUMENT_SET& docs, DocumentInfo current, RANGE r) {
  DOCUMENT_INFO_SET after_insert;

  if(!document_data) return after_insert;
  if(!load_data(current.pageno, PAGE_READWRITE)) return after_insert;

  MergeData init;
  init.src.first = 0, init.src.second = current.count-1;
  init.dst = r;
  MERGE_SET merge = get_insert_data(docs, init);

  unsigned int insert_count = r.second - r.first + 1;
  unsigned int total_count =  current.count + insert_count;

  if(total_count <= data_limit) {
    memmove(data+insert_count, data, sizeof(DocumentAddr)*current.count);
    total_count = merge_data(data, data+insert_count, docs, merge);
    
    DocumentInfo new_info = current;
    new_info.count = total_count;
    after_insert.push_back(new_info);
  } else {
    DocumentAddr* wbuf = (DocumentAddr*)malloc(sizeof(DocumentAddr)*total_count);
    total_count = merge_data(wbuf, data, docs, merge);
    if(total_count > data_limit) {
      after_insert = split_data(wbuf, total_count, current.pageno); 
    } else {
      memcpy(data, wbuf, sizeof(DocumentAddr)*total_count);
      DocumentInfo new_info = current;
      new_info.count = total_count;
      after_insert.push_back(new_info);
    }
    free(wbuf);
  }

  // keep maximum point
  if(after_insert.size() > 0) after_insert[after_insert.size()-1].max_addr = current.max_addr;

  return after_insert;
}


unsigned int DocumentController::merge_data
(DocumentAddr* wbuf, DocumentAddr* rbuf, INSERT_DOCUMENT_SET& docs, MERGE_SET& merges) {
  int read_pos = 0;
  int write_pos = 0;

  DocumentAddr tail_addr = {0, NULL_DOCUMENT};
  for(unsigned int i=0; i<merges.size(); i++) {
    if(merges[i].dst.first != -1 && merges[i].dst.second != -1) {
      int ins_pos = merges[i].dst.first;
      while(ins_pos <= merges[i].dst.second) {
        // std::cout << "insert" << ins_pos << "\n";
        if(docs[ins_pos].addr.offset == NULL_DOCUMENT) {
          docs[ins_pos].addr = document_data->insert(docs[ins_pos].data);
        } else {
          document_data->update(docs[ins_pos].addr, docs[ins_pos].data);
        }
        tail_addr = docs[ins_pos].addr;
        wbuf[write_pos++] = docs[ins_pos++].addr;
      }
    }

    if(merges[i].src.first != -1 && merges[i].src.second != -1) {
      while((int)read_pos <= merges[i].src.second && document_addr_comp(rbuf[read_pos], tail_addr) == 0) read_pos++;
      if(read_pos <= merges[i].src.second) {
        memmove(wbuf+write_pos, rbuf+read_pos, sizeof(DocumentAddr)*(merges[i].src.second-read_pos+1));
        write_pos = write_pos + (merges[i].src.second-read_pos+1);
        read_pos  = merges[i].src.second + 1;
      }
      // while((int)read_pos <= merges[i].src.second) wbuf[write_pos++] = rbuf[read_pos++];
    }
  }

  return write_pos;
}


unsigned int DocumentController::merge_info
(DocumentInfo* wbuf, DocumentInfo* rbuf, DOCUMENT_INFO_SET& infos, MERGE_SET& merges, int count)  {
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
    memcpy(wbuf+write_pos, rbuf+read_pos, sizeof(DocumentInfo)*(count-read_pos));
    write_pos = write_pos + count-read_pos;
  }

  return write_pos;
}




MERGE_SET DocumentController::get_insert_info(INSERT_DOCUMENT_SET& docs, MergeData init) {
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
          DocumentData max_data = document_data->find(info[srcmid].max_addr);
          if(max_data.id < docs[dstmid].data.id)  r=dstmid;
          else                                    l=dstmid;
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


MERGE_SET DocumentController::get_insert_data(INSERT_DOCUMENT_SET& docs, MergeData init) {
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
    RANGE dst_range = get_match_data(docs, srcmid, m.dst);

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


RANGE DocumentController::get_match_data(INSERT_DOCUMENT_SET& docs, unsigned int srcmid, RANGE range) {
  RANGE dst_range;
  int l, r;
  DocumentData d = document_data->find(data[srcmid]);

  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    if(docs[dstmid].data.id == d.id) docs[dstmid].addr = data[srcmid];

    if(docs[dstmid].data.id <= d.id) l = dstmid;
    else                             r = dstmid;
  }
  dst_range.first  = l;
  dst_range.second = r;

/*
  l = range.first-1, r = range.second+1;
  while(r-l > 1) {
    int dstmid = (l+r)/2;
    if(docs[dstmid].data.id == d.id) docs[dstmid].addr = data[srcmid];

    if(docs[dstmid].data.id > d.id) r = dstmid;
    else                            l = dstmid;
  }
  dst_range.second = r;
*/

  return dst_range;
}



bool DocumentController::remove(unsigned int doc_id) {

  return false;
}


void DocumentController::set_document_data(DocumentDataController* dp) {
  document_data   = dp;
}

DocumentDataController* DocumentController::get_document_data() {
  return document_data;
}


unsigned int DocumentController::size() {
  return 0;
}




///////////////////////////////////////////////
// private methods
///////////////////////////////////////////////
bool DocumentController::save_data() {
  data_file.save_page();
  data = NULL;

  return true;
}

bool DocumentController::save_info() {
  info_file.save_page();
  info = NULL;
 
  return true;
}


bool DocumentController::clear_data() {
  data_file.clear_page();
  data = NULL;

  return true;
}

bool DocumentController::clear_info() {
  info_file.clear_page();
  info = NULL;

  return true;
}


bool DocumentController::load_data(unsigned int pageno, int mode) {
  data = (DocumentAddr*)data_file.load_page(0, pageno, mode);
  if(!data) return false;

  return true;
}

bool DocumentController::load_info(unsigned int pageno, int mode) {
  info = (DocumentInfo*)info_file.load_page(0, pageno, mode);
  if(!info) return false;

  return true;
}


DOCUMENT_INFO_SET DocumentController::split_data(DocumentAddr* wbuf, unsigned int count, int pageno) {
  DOCUMENT_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(data, wbuf+left_pos, sizeof(DocumentAddr)*(right_pos - left_pos));
  DocumentInfo first_info = {(right_pos-left_pos), pageno, 0, wbuf[right_pos-1]};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    data_file.add_page(wbuf+left_pos, 0, header->next_data, sizeof(DocumentAddr)*(right_pos-left_pos));
    DocumentInfo add_info = {(right_pos-left_pos), header->next_data, 0, wbuf[right_pos-1]}; 
    after_info.push_back(add_info);
    header->next_data++; 
  }

  return after_info;
}


DOCUMENT_INFO_SET DocumentController::split_info(DocumentInfo* wbuf, unsigned int count, int pageno) {
  DOCUMENT_INFO_SET after_info;
  unsigned int split_pos = 0;
  for(unsigned int i=2;;i++) {
    split_pos = (count+i)/i;  // avoid zero
    if(split_pos < data_limit-1) break;
  }

  unsigned int left_pos = 0;
  unsigned int right_pos = split_pos;

  // first page
  memcpy(info, wbuf+left_pos, sizeof(DocumentInfo)*(right_pos - left_pos));
  DocumentInfo first_info = {(right_pos-left_pos), pageno, 1, wbuf[right_pos-1].max_addr};
  after_info.push_back(first_info);

  // splitted page
  while(right_pos < count) {
    left_pos = right_pos, right_pos = right_pos + split_pos;
    if(right_pos >= count) right_pos = count;

    info_file.add_page(wbuf+left_pos, 0, header->next_info, sizeof(DocumentInfo)*(right_pos-left_pos));
    DocumentInfo add_info = {(right_pos-left_pos), header->next_info, 1, wbuf[right_pos-1].max_addr}; 
    after_info.push_back(add_info);
    header->next_info++; 
  }

  return after_info;
}



bool DocumentController::init_data() {
  DOCUMENT_ADDR_SET init; 
  data_file.add_page(NULL, 0, 0);
  header->next_data = 1;

  return true;
}

bool DocumentController::init_info() {
  DOCUMENT_INFO_SET init;
  DocumentInfo first_info = {0, 0, 0,  {DATA_SECTOR_LEFT, NULL_DOCUMENT}};
  DocumentInfo second_info = {0, 0, 0,  {DATA_SECTOR_RIGHT, NULL_DOCUMENT}};
  init.push_back(first_info);
  init.push_back(second_info);
  info_file.add_page(&init[0], 0, 0, sizeof(DocumentInfo)*sizeof(init));
  header->next_info = 1;

  (header->root).max_addr.sector = DATA_SECTOR_RIGHT;
  (header->root).max_addr.offset = NULL_DOCUMENT;
  (header->root).count  = 2;
  (header->root).pageno = 0;
  (header->root).level  = 1;

  return true;
}



DocumentInfo DocumentController::find_info(unsigned long doc_id) {
  DocumentInfo err    = {0, 0, -1};
  DocumentInfo current = header->root;
 
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
        DocumentData max_data = document_data->find(info[mid].max_addr);
        if(doc_id > max_data.id) l = mid;
        else                     r = mid;
      }
    }

    current = info[r];
    if(current.level == 0)  break;
  }

  return current;
}




/////////////////////////////////////////////
//   for debug
/////////////////////////////////////////////
bool DocumentController::test() {
  unsigned int prev_data_limit = data_limit;
  unsigned int prev_info_limit = info_limit;
  data_limit = 8;
  info_limit = 8;

/*
  DOCUMENT_DATA_SET samples;
  for(unsigned int i=0; i<1000; i++) {
    DocumentData d = {i, {i, 0, 0, 0}};
    samples.push_back(d);
  }

  std::cout << "sequential inserting test...\n";
  init();
  for(unsigned int i=0; i<samples.size(); i++) {
    insert(samples[i].id, samples[i].sortkey[0]);
  }

  for(unsigned int i=0; i<samples.size(); i++) {
    DocumentData d = find(samples[i].id);
    if(d.id != samples[i].id || d.sortkey[0] != samples[i].sortkey[0]) {
      return false; 
    }
  }


  std::cout << "reverse inserting test...\n";
  shm->allocate();
  init();
  for(int i=samples.size()-1; i>=0; i--) {
    insert(samples[i].id, samples[i].sortkey[0]);
  }

  std::cout << "update test...\n";
  for(unsigned int i=0; i<samples.size(); i++) {
    insert(samples[i].id, 100);
  }

  for(unsigned int i=0; i<samples.size(); i++) {
    DocumentData d = find(samples[i].id);
    if(d.id != samples[i].id || d.sortkey[0] != 100) return false; 
  }


  std::cout << "shuffle insert test...\n";
  shm->allocate();
  init();
  for(unsigned int i=0; i<samples.size(); i=i+2) {
    insert(samples[i].id, samples[i].sortkey[0]);
  }

  for(unsigned int i=1; i<samples.size(); i=i+2) {
    insert(samples[i].id, samples[i].sortkey[0]);
  }

  for(unsigned int i=0; i<samples.size(); i++) {
    DocumentData d = find(samples[i].id);
    if(d.id != samples[i].id || d.sortkey[0] != samples[i].sortkey[0]) return false; 
  }

  std::cout << "multiple insert test...\n";
  shm->allocate();
  init();
  unsigned int cnt = samples.size();
  INSERT_DOCUMENT_SET docs;

  docs.clear();
  for(unsigned int i=0; i<cnt; i=i+2) {
    InsertDocument d = {{samples[i].id, {samples[i].sortkey[0], 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d); 
  }
  insert(docs);

  docs.clear();
  for(unsigned int i=1; i<cnt; i=i+2) {
    InsertDocument d = {{samples[i].id, {samples[i].sortkey[0], 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d); 
  }
  insert(docs);

  for(unsigned int i=0; i<cnt; i++) {
    DocumentData d = find(samples[i].id);
    if(d.id != samples[i].id || d.sortkey[0] != samples[i].sortkey[0]) return false; 
  }


  std::cout << "fizbuzz test...\n";
  shm->allocate();
  init();
  docs.clear();
  for(unsigned int i=0; i<cnt; i=i+3) {
    InsertDocument d = {{samples[i].id, {3, 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d);
  }
  insert(docs);

  docs.clear();
  for(unsigned int i=0; i<cnt; i=i+5) {
    InsertDocument d = {{samples[i].id, {5, 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d);
  }
  insert(docs);

  docs.clear();
  for(unsigned int i=0; i<cnt; i=i+7) {
    InsertDocument d = {{samples[i].id, {7, 0, 0, 0}}, {0, NULL_DOCUMENT}};
    docs.push_back(d);
  }
  insert(docs);

  for(unsigned int i=0; i<cnt; i++) {
    DocumentData d = find(samples[i].id);
    if(i%7 == 0) {
      if(d.id != samples[i].id || d.sortkey[0] != 7) return false; 
    }
    else if(i%5 == 0) {
      if(d.id != samples[i].id || d.sortkey[0] != 5) return false; 
    }
    else if(i%3 == 0) {
      if(d.id != samples[i].id || d.sortkey[0] != 3) return false; 
    }
  }
*/

  std::cout << "huge inserting test...\n";
  data_limit = prev_data_limit;
  info_limit = prev_info_limit;
  shm->init();
  init();

  // shm->x1 = shm->x2 = shm->x3 = 0;
  for(unsigned int i=0; i<100000; i++) {
    if(i%10000 == 0) std::cout << i << "...\n";
    DocumentData d = {rand()%1000000, {rand()%100, 0, 0, 0}};
    if(insert(d).offset == NULL_DOCUMENT) {
       std::cout << i << "\n";
       return false;
    }
    shm->get_header()->generation++;
  }
  // std::cout << shm->x1 << "," << shm->x2 << "," << shm->x3 << "\n";
   

  std::cout << "end process\n";
  init();

  return true;
}


void DocumentController::dump() {
  std::cout << "---dump start\n";
  dump(header->root); 
  std::cout << "---dump end\n";
}

void DocumentController::dump(DocumentInfo current) {
  if(current.level > 0) {
    for(unsigned int i=0; i<current.count; i++) {
      load_info(current.pageno, PAGE_READONLY);
      std::cout << "====node " << i << "(pageno " << info[i].pageno << ", count" << info[i].count <<  ") ====\n";
      dump(info[i]);
    }
  } else {
    load_data(current.pageno, PAGE_READONLY);
    for(unsigned int i=0; i<current.count; i++) {
      std::cout << "  document_addr: " << data[i].offset << ",";
      std::cout << "  document_id: " << find_by_addr(data[i]).id << "," << "rank: ";
      std::cout << find_by_addr(data[i]).sortkey[0] << "\n";
    } 
  }
}

