/*****************************************************************
 *  reverse_index_controller.h
 *    brief:
 *
 *  $Author: imamura $
 *  $Date:: 2009-04-07 13:03:51 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#ifndef __INDEX_CONTROLLER_H__
#define __INDEX_CONTROLLER_H__

#include <string>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "file_access.h"
#include "shared_memory_access.h"

#include "phrase_controller.h"
#include "document_controller.h"

#define REVINFO_FLAG_NONE  0x00
#define REVINFO_FLAG_LTERM 0x01
#define REVINFO_FLAG_RTERM 0x02
#define REVINFO_FLAG_EMPTY 0x04

struct ReverseIndexMerge {
  ReverseIndex* rbuf;
  ReverseIndex* wbuf;
  int read_pos;
  int write_pos;
  unsigned int ofs;
  ReverseIndex* header;
  ReverseIndex* prev;
};


class ReverseIndexController {
public:
  ReverseIndexController();
  ~ReverseIndexController();

  bool setup(std::string, SharedMemoryAccess*);
  bool init();
  bool init(std::string, SharedMemoryAccess*);
  bool clear();
  bool reset();
  bool finish();
  bool save();

  void set_document_and_phrase(DocumentController*, PhraseController*);
  PhraseController* get_phrase() {return phrase;}

  unsigned int  find(const void*, ID_SET&);
  unsigned int  find_range(const void*, SEARCH_RESULT_RANGE_SET&, unsigned short);
  unsigned int  find_prefix(const char*, ID_SET&);
  unsigned int  find_prefix_range(const char*, SEARCH_RESULT_RANGE_SET&, unsigned short);
  unsigned int  find_between(const void*, const void*, ID_SET&);
  unsigned int  find_between_range(const void*, const void*, SEARCH_RESULT_RANGE_SET&, unsigned short);

  bool insert(INSERT_REVERSE_INDEX_SET&);
 
  // for debug
  bool test(void);
  void dump(void);
  void dump(ReverseIndexInfo);
  void dump(ReverseIndex*, unsigned int);
  
  int find_hit_data_all(SEARCH_HIT_DATA_SET&, SEARCH_RESULT_RANGE_SET&, ATTR_TYPE_SET&);
  int find_hit_data_partial(SEARCH_HIT_DATA_SET&, SEARCH_RESULT_RANGE_SET&, unsigned int, ATTR_TYPE_SET&);


private:
  std::string base_path;
  FileAccess  data_file;
  FileAccess  info_file;
  SharedMemoryAccess* shm;
  Buffer buf;

  unsigned int data_limit;
  unsigned int info_limit;

  DocumentController* document;
  PhraseController* phrase;

  ReverseIndexHeader* header;
  ReverseIndex*       data;
  ReverseIndexInfo*   info;

  REVERSE_INDEX_INFO_SET insert_info(INSERT_REVERSE_INDEX_SET&, ReverseIndexInfo, RANGE r);
  REVERSE_INDEX_INFO_SET insert_data(INSERT_REVERSE_INDEX_SET&, ReverseIndexInfo, RANGE r);

  bool load_data(unsigned int, int);
  REVERSE_INDEX_INFO_SET split_data(ReverseIndex*, unsigned int, ReverseIndexInfo);
  bool save_data();
  bool clear_data();
  bool init_data();


  bool load_info(unsigned int, int);
  REVERSE_INDEX_INFO_SET split_info(ReverseIndexInfo*, unsigned int, ReverseIndexInfo);
  bool save_info();
  bool clear_info();
  bool init_info();

  MERGE_SET get_insert_info(INSERT_REVERSE_INDEX_SET&, MergeData);
  MERGE_SET get_insert_data(INSERT_REVERSE_INDEX_SET&, MergeData);
  RANGE     get_match_data(INSERT_REVERSE_INDEX_SET&, InsertReverseIndex, RANGE, bool);

  bool      init_merge_data(MERGE_SET&, MERGE_SET&, MergeData);
  bool      terminate_merge_data(MERGE_SET&, MERGE_SET&, MergeData);
  void      add_merge_data(MERGE_SET&, MergeData, RANGE, int);
  void      optimize_merge_data(MERGE_SET&);
  

  void      set_max_info(ReverseIndex*, unsigned int, ReverseIndexInfo&);

  unsigned int merge_data(ReverseIndex*, ReverseIndex*, INSERT_REVERSE_INDEX_SET&, MERGE_SET&);
  unsigned int merge_info(ReverseIndexInfo*, ReverseIndexInfo*, REVERSE_INDEX_INFO_SET&, MERGE_SET&, int);

  void levelup_root_info(REVERSE_INDEX_INFO_SET&);

  unsigned int find_info(DocumentAddr, PHRASE_ADDR_SET&, ReverseIndexInfo);
  unsigned int find_data(DocumentAddr, PHRASE_ADDR_SET&, ReverseIndexInfo);

  unsigned int         get_phrase_pos(ReverseIndex*, unsigned int);
  PhraseAddr           get_phrase_addr(ReverseIndex*, unsigned int);
  DocumentAddr         get_document_addr(ReverseIndex*, unsigned int);
  DocumentAddr         get_document_addr(ReverseIndex*, unsigned int, unsigned int);
  InsertReverseIndex   get_index_data(ReverseIndex*, unsigned int, unsigned int, char flag);

  bool is_valid_info(ReverseIndexInfo*, unsigned int);

  unsigned int search_range_to_idset(SearchResultRange&, ID_SET&);
  unsigned int find_range_common(const void*, const void*, SEARCH_RESULT_RANGE_SET&, ReverseIndexInfo, unsigned short);

  int find_left(const void*, ReverseIndexInfo, unsigned short);
  int find_right(const void*, ReverseIndexInfo, unsigned short);

  int rewrite_data(ReverseIndex*, int, int);
};


#endif // __INDEX_CONTROLLER_H__
