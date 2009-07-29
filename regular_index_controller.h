/**********************************************************************
 *  regular_index_controller.h
 *  $Author: imamura $
 *  $Date:: 2009-03-18 12:37:18 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *********************************************************************/

#ifndef __REGULAR_INDEX_H__
#define __REGULAR_INDEX_H__

#include <string>
#include <iostream>
#include <map>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "file_access.h"
#include "shared_memory_access.h"
#include "buffer.h"

#include "document_controller.h"
#include "phrase_controller.h"


class RegularIndexController {
public:
  RegularIndexController();
  ~RegularIndexController();

  bool setup(std::string, SharedMemoryAccess*);
  bool init();
  bool init(std::string, SharedMemoryAccess*);
  bool clear();
  bool reset();
  bool finish();
  bool save();


  unsigned int find_by_addr(DocumentAddr, PHRASE_ADDR_SET&);
  unsigned int find_by_id(unsigned int, PHRASE_ADDR_SET&);
  unsigned int find(DocumentAddr, PHRASE_ADDR_SET&);

  bool insert(INSERT_REGULAR_INDEX_SET&);
  unsigned int remove(InsertRegularIndex&);

  void set_document_and_phrase(DocumentController*, PhraseController*);

  // for debug
  bool test(void);
  void dump(void);
  void dump(RegularIndexInfo);
  void dump(RegularIndex*, unsigned int);

private:
  std::string base_path;
  FileAccess data_file;
  FileAccess info_file;
  SharedMemoryAccess* shm;

  unsigned int data_limit;
  unsigned int info_limit;

  PhraseController*   phrase;
  DocumentController* document;

  RegularIndexHeader* header;
  RegularIndex*       data;
  RegularIndexInfo*   info;

  REGULAR_INDEX_INFO_SET insert_info(INSERT_REGULAR_INDEX_SET&, RegularIndexInfo, RANGE r);
  REGULAR_INDEX_INFO_SET insert_data(INSERT_REGULAR_INDEX_SET&, RegularIndexInfo, RANGE r);

  bool load_data(unsigned int, int);
  REGULAR_INDEX_INFO_SET split_data(RegularIndex*, unsigned int, int);
  bool save_data();
  bool clear_data();
  bool init_data();


  bool load_info(unsigned int, int);
  REGULAR_INDEX_INFO_SET split_info(RegularIndexInfo*, unsigned int, int);
  bool save_info();
  bool clear_info();
  bool init_info();

  MERGE_SET get_insert_info(INSERT_REGULAR_INDEX_SET&, MergeData);
  MERGE_SET get_insert_data(INSERT_REGULAR_INDEX_SET&, MergeData);
  RANGE     get_match_data(INSERT_REGULAR_INDEX_SET&, unsigned int, RANGE);

  unsigned int merge_data(RegularIndex*, RegularIndex*, INSERT_REGULAR_INDEX_SET&, MERGE_SET&);
  unsigned int merge_info(RegularIndexInfo*, RegularIndexInfo*, REGULAR_INDEX_INFO_SET&, MERGE_SET&, int);

  DocumentAddr get_document_addr(RegularIndex*, unsigned int);
  PhraseAddr   get_phrase_addr(RegularIndex*, unsigned int);
  PhraseAddr   get_phrase_addr(RegularIndex*, unsigned int, unsigned int);

  void levelup_root_info(REGULAR_INDEX_INFO_SET&);

  unsigned int find_info(DocumentAddr, PHRASE_ADDR_SET&, RegularIndexInfo);
  unsigned int find_data(DocumentAddr, PHRASE_ADDR_SET&, RegularIndexInfo);
 
  unsigned int remove_info(InsertRegularIndex&, RegularIndexInfo);
  unsigned int remove_data(InsertRegularIndex&, RegularIndexInfo);
};

#endif // __REGULAR_INDEX_H__
