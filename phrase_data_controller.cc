/*****************************************************************
 *   phrase controller.cc
 *     brief: Phrase table operation.
 *
 *   $Author: imamura $
 *   $Date:: 2009-03-18 12:37:18 +0900#$
 *
 *   Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

#include <iostream>
#include "phrase_data_controller.h"


//////////////////////////////////////////
//  constructor & destuctor
//////////////////////////////////////////
PhraseDataController::PhraseDataController() {
  base_path = "";
  data = NULL;
  shm = NULL;
  header = NULL;
}

PhraseDataController::~PhraseDataController() {
  data = NULL;
  shm = NULL;
  header = NULL;
}


//////////////////////////////////////////
//  public methods
//////////////////////////////////////////
bool PhraseDataController::setup(std::string path, SharedMemoryAccess* _shm) {
  if(!FileAccess::is_directory(path)) return false;
  if(!_shm) return false;

  base_path = path;
  shm = _shm;

  data_file.set_file_name(path, DATA_TYPE_PHRASE_DATA, "dat");
  data_file.set_shared_memory(shm);

  header = &(shm->get_header()->p_header);

  offset_limit= shm->get_page_size();
  sector_limit = DEFAULT_PHRASE_SECTOR_LIMIT;
  
  return true;
}

bool PhraseDataController::init(std::string path, SharedMemoryAccess* _shm) { // with setting
  if(!setup(path, _shm)) return false;
  return init();
}

bool PhraseDataController::init() { // without setting
  return clear();
}

bool PhraseDataController::reset() {
  clear_page();
  return true;
}

bool PhraseDataController::clear() {
  clear_page();

  data_file.remove_with_suffix();
  (header->next_addr).offset = 0;
  (header->next_addr).sector = 0;

  return true;
}

bool PhraseDataController::finish() {
  clear_page();
  return true;
}


///////////////////////////////////////////////////////////////////

PhraseData PhraseDataController::find(PhraseAddr search) {
  PhraseAddr& next_addr = (header->next_addr);
  PhraseData err = {NULL};

  if(search.offset == NULL_PHRASE)  return err;

  if(search.sector > next_addr.sector || (search.sector == next_addr.sector && search.offset >= next_addr.offset)) {
    throw AppException(EX_APP_PHRASE, "invalid offset");
  }

  unsigned int page = search.offset/offset_limit;
  if(!load_page(search.sector, page, PAGE_READONLY)) {
    std::cout << "file map failed: " << page << "\n";
    return err;
  }

  PhraseData result = {(char*)data + search.offset % offset_limit};

  return result;
}

PhraseData PhraseDataController::find(PhraseAddr search, Buffer& buf) {
  PhraseAddr& next_addr = (header->next_addr);
  PhraseData err = {NULL};

  if(search.offset == NULL_PHRASE)  return err;

  if(search.sector > next_addr.sector || (search.sector == next_addr.sector && search.offset >= next_addr.offset)) {
    throw AppException(EX_APP_PHRASE, "invalid offset");
  }

  unsigned int page = search.offset/offset_limit;
  if(!load_page(search.sector, page, PAGE_READONLY)) {
    std::cout << "file map failed: " << page << "\n";
    return err;
  }

  char* c = (char*)data + (search.offset % offset_limit);
  int len = IS_ATTR_TYPE_STRING(c[0]) ? strlen(c+1)+2 : sizeof(int)+1;

  PhraseData result = {buf.allocate(len)};
  memcpy(result.value, c, len);

  return result;
}



PhraseAddr PhraseDataController::insert(InsertPhrase& ins) {
  return insert(ins.data.value[0], ins.data.value+1);
}


PhraseAddr PhraseDataController::insert(char type, const void* value) {
  PhraseAddr& next_addr = (header->next_addr);
  PhraseAddr err = {0, NULL_PHRASE};
  if(next_addr.sector > MAX_SECTOR) return err;

  unsigned int length = 0;
  if(IS_ATTR_TYPE_STRING(type))   length = strlen((const char*)value) + 1;
  else                            length = sizeof(int);

  if(length > MAX_PHRASE_LENGTH) return err;

  if(next_addr.offset % offset_limit == 0 || next_addr.offset/offset_limit < (next_addr.offset+length)/offset_limit) {
    unsigned int next_addr_pageno = (next_addr.offset + length)/offset_limit;
    char buf[length+1];
    buf[0] = type; 
    memcpy(&buf[1], value, length);
    new_page(buf, next_addr.sector, next_addr_pageno, length);
    next_addr.offset = next_addr_pageno * offset_limit;
  } else {
    load_page(next_addr.sector, next_addr.offset/offset_limit, PAGE_READWRITE);
    data[next_addr.offset % offset_limit] = type;
    memcpy(data+(next_addr.offset % offset_limit)+1, value, length);
  }

  PhraseAddr current = next_addr;
  next_addr.offset =  next_addr.offset + length + 1;
  if(next_addr.offset > sector_limit) {
    next_addr.sector++;
    next_addr.offset = 0;
  }
  save();

  return current;
}


PhraseAddr PhraseDataController::insert(const char* str) {
  return insert(ATTR_TYPE_STRING, str);
}

PhraseAddr PhraseDataController::insert(int val) {
  return insert(ATTR_TYPE_INTEGER, &val);
}


bool PhraseDataController :: save() {
  save_page();
  return true;
}


int PhraseDataController::compare(const char* src, PhraseData d) {
  if(!d.value) return 1;

  unsigned char type = (unsigned char)d.value[0];
  if(type != ATTR_TYPE_STRING) {
    return (type > ATTR_TYPE_STRING) ? -1 : 1;
  }

  return strcmp(src, d.value+1);
}

int PhraseDataController::compare(int src, PhraseData d) {
  if(!d.value) return 1;

  unsigned char type = (unsigned char)d.value[0];
  if(type != ATTR_TYPE_INTEGER) {
    return (type > ATTR_TYPE_STRING) ? -1 : 1;
  }

  int dst = *(const int*)(d.value + 1);
  return (src < dst ? -1 : (src > dst ? 1 : 0));
}

int PhraseDataController::compare(PhraseData a, PhraseData b) {
  if(!a.value && !b.value) return 0;
  if(!a.value)  return -1;
  if(!b.value)  return 1;

  if(a.value[0] != b.value[0]) {
    return (a.value[0] > b.value[0] ? 1 : -1);
  }

  if(IS_ATTR_TYPE_STRING(a.value[0])) {
    return strcmp(a.value+1, b.value+1);
  }
  else {
    if(*(int*)(a.value+1) < *(int*)(b.value+1)) return -1;
    else if(*(int*)(a.value+1) > *(int*)(b.value+1)) return 1;
    else return 0;
  }


  return 0;
}


//////////////////////////////////////////
//  private methods
//////////////////////////////////////////
bool PhraseDataController::new_page(const void* head_data, unsigned short secno, unsigned int pageno, unsigned int length) {
  save_page();
  data_file.add_page(head_data, secno, pageno, length);

  return true;
}


bool PhraseDataController::load_page (unsigned short secno, unsigned int pageno, int mode) {
  data = (char*)data_file.load_page(secno, pageno, mode); 
  if(!data) return false;

  return true;
}

void PhraseDataController :: save_page() {
  data_file.save_page();
  data = NULL;
}

void PhraseDataController :: clear_page() {
  data_file.clear_page();
  data = NULL;
}


/////////////////////////////////////////////////
// for debug
/////////////////////////////////////////////////
bool PhraseDataController :: test() { 
  Buffer buf;

  std::cout << "initialize test...\n";
  if(!init()) {
    std::cout << "failed\n"; 
    return false;
  }

  PhraseAddr   addr;
  offset_limit = 50;
  sector_limit = 1000;

  std::cout << "insert string test...\n";
  addr = insert("firstdata");
  if(compare("firstdata", find(addr, buf))) return false;

  std::cout << "insert integer test ...\n";
  addr = insert(1223);
  if(compare(1223, find(addr, buf))) return false;


  for(unsigned int i=0; i<5; i++) insert("grape");

  std::cout << "empty string test...\n";
  addr = insert("");

  std::cout << "middle find test...\n";
  for(unsigned int i=0; i<3; i++) insert("apple");
  addr = insert("findstr");
  for(unsigned int i=0; i<3; i++) insert("orange");

  std::cout << "long data test...\n";
  std::string longstr = ""; 
  for(unsigned int i=0; i<99; i++) {
    longstr = longstr + "0123456789";
  } 
  addr = insert(longstr.c_str());
  if(compare(longstr.c_str(), find(addr, buf))) return false;

  std::cout << "too long data test...\n";
  longstr = longstr + "0123456789";
  addr = insert(longstr.c_str());
  if(addr.offset != NULL_PHRASE) return false;

  std::cout << "sector switch test...\n";
  init();
  for(unsigned int i=0; i<200; i++) {
    insert("abcdefg");
  }
  addr = insert("test");
  if(addr.sector != 1) return false;

  std::cout << "too many data test...\n";
  (header->next_addr).sector = MAX_SECTOR+1;
  (header->next_addr).offset = 0;
  addr = insert("test");
  if(addr.offset != NULL_DOCUMENT) return false;

  std::cout << "end process...\n";

  return true;
}

void PhraseDataController::dump() {
  PhraseAddr& next_addr = header->next_addr;
  std::cout << "---dump start\n";
  for(unsigned int i=0; i<=next_addr.sector; i++) {
    unsigned int ofs = 0;
    unsigned int max_ofs = i==next_addr.sector ? next_addr.offset : sector_limit;
    unsigned int page = 0xFFFFFFFF;

    std::cout << "[sector " << i << "]\n";
    while(ofs < max_ofs) {
      if(page != ofs/offset_limit) { 
        if(!load_page(i, ofs/offset_limit, PAGE_READONLY)) break;
        page = ofs/offset_limit;
        std::cout << "  (page " << page << ")\n";
      }

      unsigned char type =  (unsigned char)data[ofs%offset_limit];
      void* value = data+ofs%offset_limit+1;
      if(IS_ATTR_TYPE_STRING(type)) {
        std::cout << "    (string)" << (char*)value << "\n";
        ofs = ofs + strlen((char*)value) + 2;
      }
      else {
        std::cout << "    (integer)" << *((int*)value) << "\n";
        ofs = ofs + 1 + sizeof(int);
      }
    }
  } 

  std::cout << "---dump end\n";
}

