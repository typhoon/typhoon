/*****************************************************************
 *  file_access.h
 *    brief: File access and mapping class and functions.
 *
 *  $Author: imamura $
 *  $Date:: 2009-04-05 15:22:53 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#ifndef __FILE_ACCESS_H__
#define __FILE_ACCESS_H__

#include <string>
#include <iostream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "exception.h"
#include "shared_memory_access.h"

#define MAX_FILE_SIZE   1000000000
#define MAX_SECTOR_SIZE 0x7FFF

#define PAGE_NONE 0
#define PAGE_READONLY 1
#define PAGE_READWRITE 2

#define EMPTY_BUFFER_SIZE 4096

typedef std::vector<struct FileHandler>  FILE_HANDLER_SET;

struct PageInfo {
  unsigned short secno;
  int            pageno;
  int            mode;
  unsigned int   type;
};

struct FileHandler {
  int   secno;
  int   fileno;
  int   fd;
};


class FileAccess {
public:
  ~FileAccess();
  FileAccess();
  FileAccess(std::string);

  // file system operations 
  static bool is_directory(std::string);
  static bool is_file(std::string);
  bool        is_file();

  static bool create_directory(std::string);
  bool        create();

  static bool rename(std::string, std::string);
  bool        rename(std::string);

  static bool remove(std::string);
  bool        remove();

  static size_t get_file_length(std::string&);
  static size_t get_file_length(const char*);
  size_t        get_file_length();

  void  set_file_name(std::string);
  void  set_file_name(std::string, unsigned int, std::string);
  std::string get_file_name();

  WORD_SET get_suffix_files(void);
  bool   remove_with_suffix();

  unsigned int get_page_size();
  void set_page_size(unsigned int);

  void  set_shared_memory(class SharedMemoryAccess* shm);
  class SharedMemoryAccess*  get_shared_memory();


  bool  add_page(const void*, unsigned short, unsigned int, unsigned int);
  bool  add_page(const void*, unsigned short, unsigned int);
  void* load_page(unsigned short, unsigned int, int);
  void* map_page(unsigned short, unsigned int, int); 
  bool  save_page();
  bool  clear_page();
  bool  write_page(void*, unsigned int);
   

  bool  set_page_info(unsigned short, unsigned int, int);

  FileHandler get_handler(PageInfo);
  void        clear_handler();

  static void empty_buffer_init();

  bool test();

private:
  std::string   file_name;
  unsigned int               page_size;
  unsigned int               page_carry;
  void*                      page_ptr;
  struct PageInfo            page_info;
  class  SharedMemoryAccess* shm;
  FILE_HANDLER_SET           handlers;

  bool clear_page_info();

  static char empty_buffer[EMPTY_BUFFER_SIZE];
};



#endif // __HUGE_FILE_ACCESS_H__
