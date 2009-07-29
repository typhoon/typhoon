/*****************************************************************
 *  searcher.h
 *    brief:
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-23 14:01:30 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#ifndef __SEARCHER_H__
#define __SEARCHER_H__

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "data_controller.h"
#include "app_config.h"
#include "shared_memory_access.h"
#include "buffer.h"
#include "indexer.h"


class Searcher {
public:
  int offset;
  int limit;

  Searcher();
  Searcher(std::string, std::string, ATTR_TYPE_MAP*, SharedMemoryAccess*);
  ~Searcher();

  void init();
  void setup(std::string, std::string, ATTR_TYPE_MAP*, SharedMemoryAccess*);

  bool parse_request(JsonValue*, AppConfig&);
  int  do_search(SEARCH_HIT_DATA_SET&);
  bool search(JsonValue*, JsonValue*, AppConfig&);
  bool match(JsonValue*, JsonValue*, AppConfig&);

  bool test();
  void dump();

private:
  std::string path;
  std::string log_file;

  ATTR_TYPE_MAP* attrs;
  SharedMemoryAccess* shm;

  bool lazy_count;

  DataController   data;
  Buffer           buf;
  SEARCH_NODE_SET  nodes;
  int root_node;
  SEARCH_CACHE_SET caches;
  ATTR_TYPE_SET    order;

  int         parse_conditions(JsonValue*);
  int         parse_conditions_level1(JsonValue*);
  int         parse_conditions_level2(JsonValue*);
  int         parse_conditions_leaf(JsonValue*);
  int         parse_conditions_fulltext(JsonValue*, std::string, AttrDataType);
  char*       parse_conditions_index(std::string, AttrDataType, JsonValue* val);
  bool        parse_order(JsonValue*);


  SearchHitData pickup_hit(int);
  void          clear_current_hit(int);
  SearchHitData pickup_cache(int);
  void          setup_cache();
  void          setup_node();
};

  

#endif // __SEARCHER_H__
