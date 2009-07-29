/*****************************************************************
 *  index.h
 *    brief:
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 19:53:02 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#ifndef __INDEXER_H__
#define __INDEXER_H__

#include <string>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "app_config.h"
#include "common.h"
#include "morph_controller.h"
#include "buffer.h"
#include "shared_memory_access.h"

#include "data_controller.h"


class Indexer {
public:
  Indexer();
  Indexer(std::string, std::string, ATTR_TYPE_MAP*, SharedMemoryAccess*, MorphController*, Buffer*);
  ~Indexer();

  DataController data;

  void setup(std::string, std::string, ATTR_TYPE_MAP*, SharedMemoryAccess*, MorphController*, Buffer*);

  bool do_index(INSERT_REGULAR_INDEX_SET&);

  bool parse_request(InsertRegularIndex&, JsonValue*);

  bool proc_remove_indexes(INSERT_REGULAR_INDEX_SET&);
  bool proc_insert_phrases(INSERT_REGULAR_INDEX_SET&);
  bool proc_insert_documents(INSERT_REGULAR_INDEX_SET&);
  bool proc_insert_regular_indexes(INSERT_REGULAR_INDEX_SET&);
  bool proc_insert_reverse_indexes(INSERT_REGULAR_INDEX_SET&);
  bool proc_sector_check();

  bool save_source_document(InsertRegularIndex&);
  bool load_source_documents(INSERT_REGULAR_INDEX_SET&);

  bool test();

private:
  std::string path;
  std::string log_file;

  ATTR_TYPE_MAP*      attrs;
  SharedMemoryAccess* shm;
  MorphController*    morph;
  Buffer*             buf;

  PhraseAddr   find_phrase_addr_by_cache(PhraseData, INSERT_PHRASE_SET&);
  DocumentAddr find_document_addr_by_cache(DocumentData, INSERT_DOCUMENT_SET&);

  void        set_default_phrase(INSERT_PHRASE_SET&, std::string&, JsonValue*);
  void        set_attr_phrase(INSERT_PHRASE_SET&, AttrDataType, std::string&, JsonValue*);
  std::string get_string_attr(JsonValue*);
  int         get_integer_attr(JsonValue*);
  char*       get_phrase_data(unsigned char, const void*, unsigned int);

  void        set_fulltext_phrase(INSERT_PHRASE_SET&, AttrDataType, std::string&, JsonValue*);
  void        set_phrase_pos(INSERT_PHRASE_SET&);
  void        set_content_line(INSERT_PHRASE_SET&, char*, AttrDataType, std::string&);

  std::string get_timer_suffix();
  std::string get_source_data_file(std::string);

  void to_unique_document(INSERT_REGULAR_INDEX_SET&);
};


#endif // __INDEXER_H__
