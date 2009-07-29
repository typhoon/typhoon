/**********************************************************************
 *  document_data_controller.h
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 12:43:26 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *********************************************************************/

#ifndef __DOCUMENT_DATA_H__
#define __DOCUMENT_DATA_H__

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

#define DEFAULT_DOCUMENT_SECTOR_LIMIT 0x00FFFFFF

class DocumentDataController {
public:
  DocumentDataController();
  ~DocumentDataController();

  bool init();
  bool init(std::string, SharedMemoryAccess*);
  bool reset();
  bool clear();
  bool save();
  bool finish();
  bool setup(std::string, SharedMemoryAccess*);

  DocumentData find(DocumentAddr);
  DocumentAddr insert(DocumentData);
  bool update(DocumentAddr, DocumentData);

  DocumentAddr get_next_addr();
  void         set_next_sector();

  // for debug
  bool test(void);
  void dump(void);

private:
  std::string base_path;
  FileAccess data_file;
  SharedMemoryAccess* shm;

  unsigned int offset_limit;
  unsigned int sector_limit;

  DocumentData*     data;
  DocumentHeader*   header;

  bool load_page(unsigned short, unsigned int, int);
  bool new_page(DocumentData, unsigned short, unsigned int);
  bool save_page();
  bool clear_page();  
};

#endif // __DOCUMENT_H__
