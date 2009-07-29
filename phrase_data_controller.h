/*****************************************************************
 *  phrase_data_controller.cc
 *    brief:
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 12:37:18 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#ifndef __PHRASE_DATA_H__
#define __PHRASE_DATA_H__

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
#include "buffer.h"
#include "file_access.h"
#include "exception.h"
#include "shared_memory_access.h"

#define DEFAULT_PHRASE_SECTOR_LIMIT 0x0FFFFFFF 


class PhraseDataController {
public:
  PhraseDataController();
  ~PhraseDataController();

  bool            init();
  bool            init(std::string, SharedMemoryAccess*);
  bool            clear(void);
  bool            reset(void);
  bool            save(void);
  bool            finish(void);
  bool            setup(std::string, SharedMemoryAccess*);


  PhraseAddr  insert(InsertPhrase&);
  PhraseAddr  insert(char, const void*);
  PhraseAddr  insert(const char*);
  PhraseAddr  insert(int);
  PhraseData  find(PhraseAddr);
  PhraseData  find(PhraseAddr, Buffer&);

  // debug
  bool            test();
  void            dump();
  std::string base_path;

  // static
  static int compare(PhraseData, PhraseData);
  static int compare(const char*, PhraseData);
  static int compare(int, PhraseData);
private:
  FileAccess  data_file;
  SharedMemoryAccess* shm;

  PhraseHeader*   header;
  char*           data;
  unsigned int    offset_limit;
  unsigned int    sector_limit;

  bool new_page(const void*, unsigned short, unsigned int, unsigned int);
  bool load_page(unsigned short, unsigned int, int);
  void save_page();
  void clear_page();
};

#endif // __PHRASE_H__
