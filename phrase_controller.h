/*****************************************************************
 *  phrase_controller.h
 *    brief:
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 12:37:18 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#ifndef __PHRASE_H__
#define __PHRASE_H__

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
#include "phrase_data_controller.h"
#include "buffer.h"

class PhraseController {
public:
  PhraseController();
  ~PhraseController();

  bool            setup(std::string, SharedMemoryAccess*);
  bool            init();
  bool            init(std::string, SharedMemoryAccess*);
  bool            clear(void);
  bool            finish(void);
  bool            reset(void);
  bool            save(void);

  PhraseAddr    insert(const char*);
  PhraseAddr    insert(const std::string&);
  PhraseAddr    insert(int);
  PhraseAddr    insert(PhraseData);
  bool          insert(INSERT_PHRASE_SET&);

  PhraseData find_data(PhraseAddr, Buffer&);
  PhraseData find_data_by_addr(PhraseAddr, Buffer&);
  PhraseAddr find_addr(PhraseData);
  PhraseAddr find_addr(const char*);
  PhraseAddr find_addr_by_string(const char*);

  void set_phrase_data(PhraseDataController*);

  // debug
  bool            test();
  void            dump();
  void            dump(PhraseInfo); 

private:
  std::string base_path;
  FileAccess data_file;
  FileAccess info_file;
  SharedMemoryAccess* shm;

  PhraseDataController* phrase_data;

  PhraseHeader*   header;
  PhraseInfo*     info;
  PhraseAddr*     data;
  unsigned int    data_limit;
  unsigned int    info_limit;

  PHRASE_INFO_SET insert_info(INSERT_PHRASE_SET&, PhraseInfo, RANGE r);
  PHRASE_INFO_SET insert_data(INSERT_PHRASE_SET&, PhraseInfo, RANGE r);

  bool load_data(unsigned int, int);
  PHRASE_INFO_SET split_data(PhraseAddr*, unsigned int, int);
  bool save_data();
  bool clear_data();
  bool init_data();
  

  bool load_info(unsigned int, int);
  PHRASE_INFO_SET split_info(PhraseInfo*, unsigned int, int);
  bool save_info();
  bool clear_info();
  bool init_info();

  PhraseInfo find_info(PhraseData);
  MERGE_SET get_insert_info(INSERT_PHRASE_SET&, MergeData);
  MERGE_SET get_insert_data(INSERT_PHRASE_SET&, MergeData);
  RANGE     get_match_data(INSERT_PHRASE_SET&, unsigned int, RANGE);

  unsigned int merge_data(PhraseAddr*, PhraseAddr*, INSERT_PHRASE_SET&, MERGE_SET&);
  unsigned int merge_info(PhraseInfo*, PhraseInfo*, PHRASE_INFO_SET&, MERGE_SET&, int);

  bool find_index_by_id(const char* phrase, unsigned int& idx);
  bool find_index_by_id(PhraseData, unsigned int& idx);

  PhraseData find_data(PhraseAddr);
  PhraseData find_data_by_addr(PhraseAddr);

};


#endif // __PHRASE_H__
