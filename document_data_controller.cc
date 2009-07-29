/*****************************************************************
 *   document_data_controller.cc
 *     brief: Document-phrase mapping data.
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 12:43:26 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>

#include "document_data_controller.h"

/////////////////////////////////////////////
// constructor & destructor
/////////////////////////////////////////////
DocumentDataController::DocumentDataController() {
  data = NULL;
  shm = NULL;
  header = NULL;
}

DocumentDataController::~DocumentDataController() {
  data = NULL;
  shm = NULL;
  header = NULL;
}



/////////////////////////////////////////////
// public methods
/////////////////////////////////////////////
bool DocumentDataController::init(std::string path, SharedMemoryAccess* _shm) { // with settings
  if(!setup(path, _shm)) return false;
  return init();
}

bool DocumentDataController::init() { // without settings
  return clear();
}

bool DocumentDataController::clear() {
  clear_page();

  data_file.remove_with_suffix();
  (header->next_addr).offset = 0;
  (header->next_addr).sector = 0;

  return true;
}


bool DocumentDataController::reset() {
  clear_page();
  return true;
}

bool DocumentDataController::save() {
  save_page();
  return true;
}


bool DocumentDataController::finish() {
  clear_page();
  return true;
}



bool DocumentDataController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  base_path = path;
  shm = _shm;

  data_file.set_file_name(path, DATA_TYPE_DOCUMENT_DATA, "dat");
  data_file.set_shared_memory(shm);

  offset_limit = shm->get_page_size() / sizeof(DocumentData);
  sector_limit = DEFAULT_DOCUMENT_SECTOR_LIMIT;

  header = &(shm->get_header()->d_header);

  return true;
}


DocumentData DocumentDataController::find(DocumentAddr find_addr) {
  DocumentAddr& next_addr = header->next_addr;
  DocumentData emptydata = {0, {0, 0, 0, 0}};

  if(find_addr.offset == NULL_DOCUMENT) return emptydata;

  if(find_addr.sector > next_addr.sector || (find_addr.sector == next_addr.sector && find_addr.offset >= next_addr.offset)) {
    throw AppException(EX_APP_DOCUMENT, "invalid document addr");
    // std::cout << "[" << find_addr.sector << "," << find_addr.offset << "]" << " invalid addr(max: " << next_addr.sector << ")\n";
    return emptydata;
  }

  unsigned int page = find_addr.offset/offset_limit;
  if(!load_page(find_addr.sector, page, PAGE_READONLY)) {
    std::cout << "file map failed: " << page << "\n";
    return emptydata;
  }

  return data[find_addr.offset%offset_limit];
}


DocumentAddr DocumentDataController::insert(DocumentData insert_data) {
  DocumentAddr& next_addr = header->next_addr;
  DocumentAddr return_addr = {0, NULL_DOCUMENT};

  if(next_addr.sector > MAX_SECTOR) return return_addr;
  if(next_addr.offset == NULL_DOCUMENT || next_addr.offset % offset_limit == 0) {
    unsigned int next_pageno = next_addr.offset / offset_limit;
    new_page(insert_data, next_addr.sector, next_pageno);
  } else {
    load_page(next_addr.sector, next_addr.offset/offset_limit, PAGE_READWRITE);
    data[next_addr.offset%offset_limit] = insert_data;
  }

  return_addr = next_addr;

  next_addr.offset++;
  if(next_addr.offset >= sector_limit) {
    set_next_sector();
  }

  save();
 
  return return_addr;
}

bool DocumentDataController::update(DocumentAddr up_addr, DocumentData up_data) {
  DocumentAddr& next_addr = header->next_addr;
  if(up_addr.sector > next_addr.sector || (up_addr.sector == next_addr.sector && up_addr.offset >= next_addr.offset)) return false;

  unsigned int page = up_addr.offset/offset_limit;
  if(!load_page(up_addr.sector, page, PAGE_READWRITE)) return false;

  data[up_addr.offset%offset_limit] = up_data;
  return true;
}


DocumentAddr DocumentDataController::get_next_addr() {
  return header->next_addr;
}

void DocumentDataController::set_next_sector() {
  header->next_addr.sector++;
  header->next_addr.offset = 0;
}


///////////////////////////////////////////////
// private methods
///////////////////////////////////////////////
bool DocumentDataController::save_page() {
  data_file.save_page();
  data = NULL;

  return true;
}

bool DocumentDataController::clear_page() {
  data_file.clear_page();
  data = NULL;
  return true;
}


bool DocumentDataController::load_page(unsigned short secno, unsigned int pageno, int mode) {
  data = (DocumentData*)data_file.load_page(secno, pageno, mode);
  if(!data) return false;

  return true;
}


bool DocumentDataController::new_page(DocumentData first_data, unsigned short secno, unsigned int pageno) {
  save_page();
  data_file.add_page(&first_data, secno, pageno, sizeof(DocumentData));

  return true;
}



/////////////////////////////////////////////
//   for debug
/////////////////////////////////////////////
bool DocumentDataController::test() {
  offset_limit = 10;
  sector_limit = 100;
  DOCUMENT_DATA_SET testdata;

  std::cout << "insert and find test...\n";
  init();
  for(unsigned int i=0; i<sector_limit; i++) {
    DocumentData d = {i, {i, 0, 0, 0}};
    testdata.push_back(d);
  }

  DocumentAddr search_addr;

  search_addr = insert(testdata[0]);
  for(unsigned int i=1; i<testdata.size(); i++) {
    DocumentAddr insert_addr = insert(testdata[i]);
    if(insert_addr.offset == NULL_DOCUMENT) return false;
    DocumentData doc = find(search_addr);
    if(doc.id != testdata[0].id) return false;
  }


  std::cout << "sector change test...\n";
  testdata.clear();
  init();
  for(unsigned int i=0; i<sector_limit; i++) {
    DocumentData d = {i, {i, 0, 0, 0}};
    DocumentAddr insert_addr = insert(d);
  }
  DocumentData d = {sector_limit, {0,  0, 0, 0}};
  search_addr = insert(d);
  if(search_addr.sector != 1)  return false;
  

  std::cout << "limit over test...\n";
  (header->next_addr).sector = MAX_SECTOR+1;
  (header->next_addr).offset = 0;
  DocumentData d2 = {0, {0, 0, 0, 0}};
  search_addr = insert(d2);
  if(search_addr.offset != NULL_DOCUMENT) return false;


  init();
  std::cout << "end process\n";
  return true;
}


void DocumentDataController::dump() {
  DocumentAddr& next_addr = header->next_addr;
  std::cout << "---document data dump\n";
  for(unsigned short s=0; s<=next_addr.sector; s++) { 
    std::cout << "[sector " << s << "]\n";
    unsigned int max_addr = (s==next_addr.sector) ? next_addr.offset : sector_limit;
    for(unsigned int i=0; i<max_addr; i++) {
      DocumentAddr addr = {s, i};
      DocumentData d = find(addr);
      if(d.id == NULL_DOCUMENT) {
        std::cout << i << ":" << "empty_data\n";
      }
      else {
        std::cout << i << ":" << d.id << "," << d.sortkey[0] << "\n";
      }
    }
  }
  std::cout << "---dump end\n";
}


