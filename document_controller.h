/********************************************************************************
 *   document_controller.h
 *     brief: Document data consists of three parts, document base data,
 *            Document base data is composed of original DOCUMENT_ID and rank placed on fixed address.
 *            (We can expand this data format.)
 *            that is sorted by base
 *            Document data is needed fixed address access and original ID based access,
 *            then we split base data and address data.
 *      Note: If we use original ID instead of fixed address, we needs O(logN) binary-search
 *            when calling document data from index data. Call by address is O(1).
 *  $Author: imamura $
 *  $Date:: 2009-04-07 13:03:51 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *******************************************************************************/


#ifndef __DOCUMENT_H__
#define __DOCUMENT_H__

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
#include "document_data_controller.h"
#include "exception.h"

class DocumentController {
public:
  DocumentController();
  ~DocumentController();

  bool init();
  bool init(std::string, SharedMemoryAccess*);
  bool clear();
  bool reset();
  bool save();
  bool finish();
  bool setup(std::string, SharedMemoryAccess*);

  DocumentData find(unsigned int);
  DocumentData find_by_id(unsigned int);
  DocumentData find_by_addr(DocumentAddr);
  DocumentAddr find_addr(unsigned int);
  DocumentAddr find_addr_by_id(unsigned int);

  DocumentAddr insert(DocumentData);
  bool         insert(INSERT_DOCUMENT_SET&);
  bool         remove(unsigned int);
  unsigned int size();

  // for debug
  bool test(void);
  void dump(void);
  void dump(DocumentInfo);

  void set_document_data(DocumentDataController*);
  DocumentDataController* get_document_data();

private:
  std::string base_path;
  SharedMemoryAccess* shm;
  DocumentDataController*   document_data;
 

  FileAccess data_file;
  FileAccess info_file;

  unsigned int data_limit;
  unsigned int info_limit;

  DocumentHeader*  header;
  DocumentAddr*    data;
  DocumentInfo*    info;

  DOCUMENT_INFO_SET insert_info(INSERT_DOCUMENT_SET&, DocumentInfo, RANGE r);
  DOCUMENT_INFO_SET insert_data(INSERT_DOCUMENT_SET&, DocumentInfo, RANGE r); 

  bool load_data(unsigned int, int);
  DOCUMENT_INFO_SET split_data(DocumentAddr*, unsigned int, int);
  bool save_data();
  bool clear_data();
  bool init_data();

  bool load_info(unsigned int, int);
  DOCUMENT_INFO_SET split_info(DocumentInfo*, unsigned int, int);
  bool save_info();
  bool clear_info();
  bool init_info();

  DocumentInfo find_info(unsigned long);
  MERGE_SET get_insert_info(INSERT_DOCUMENT_SET&, MergeData); 
  MERGE_SET get_insert_data(INSERT_DOCUMENT_SET&, MergeData);
  RANGE     get_match_data(INSERT_DOCUMENT_SET&, unsigned int, RANGE);

  unsigned int merge_data(DocumentAddr*, DocumentAddr*, INSERT_DOCUMENT_SET&, MERGE_SET&);
  unsigned int merge_info(DocumentInfo*, DocumentInfo*, DOCUMENT_INFO_SET&, MERGE_SET&, int);
};

#endif // __DOCUMENT_H__
