/*****************************************************************
 *  file_access.cc
 *    brief: File access and mapping class.
 *
 *  $Author: imamura $
 *  $Date:: 2009-04-05 23:27:47 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#include <iostream>
#include "common.h"
#include "file_access.h"

using namespace std;

char FileAccess::empty_buffer[EMPTY_BUFFER_SIZE];

///////////////////////////////////////////////////////////
// constructor / destructor
//
FileAccess::FileAccess() {
  shm = NULL;
  page_ptr = NULL;
  page_size = 0;
  page_carry = 1;
  page_info.type = DATA_TYPE_DEFAULT;

  clear_page_info();
}

FileAccess::FileAccess(std::string name) {
  shm = NULL;
  page_ptr = NULL;
  page_carry = 1;
  page_info.type = DATA_TYPE_DEFAULT;

  set_file_name(name);
  clear_page_info();
}


FileAccess::~FileAccess() {
  page_ptr = NULL;
  shm = NULL;

  clear_handler();
}



bool FileAccess::is_file() {
  return is_file(file_name);
}

bool FileAccess::is_file(std::string path) {
  struct stat st;
  if(stat(path.c_str(), &st) == -1)  return false;
  return S_ISREG(st.st_mode);
}


bool FileAccess::create() {
  if(is_file()) return false;
  FILE* fp = fopen(file_name.c_str(), "wb");
  if(!fp) return false;
  fclose(fp);
  return true;
}


bool FileAccess::remove_with_suffix() {
  clear_page();

  WORD_SET suffix_files = get_suffix_files();
  for(unsigned int i=0; i<suffix_files.size(); i++) { 
    remove(suffix_files[i].c_str());
  }

  return true;
}

WORD_SET FileAccess::get_suffix_files() {
  WORD_SET suffix_files;
  WORD_SET pathlist = split(file_name, "/");
  if(pathlist.size() == 0) return suffix_files;
  std::string base_file = pathlist[pathlist.size()-1] + ".";
  pathlist.pop_back();
  std::string base_path = join(pathlist, "/");

  DIR* dp;
  struct dirent *dir;
  dp = opendir(base_path.c_str());
  if(!dp) return suffix_files;

  while((dir = readdir(dp)) != NULL) {
    if(strncmp(dir->d_name, base_file.c_str(), base_file.length()) == 0) {
      suffix_files.push_back(base_path + "/" + dir->d_name);
    }
  }
  closedir(dp);

  return suffix_files;
}


void FileAccess::set_file_name(std::string name) {
  clear_page();
  file_name = name;
}


void FileAccess::set_file_name(std::string path, unsigned int data_type, std::string suffix) {
  clear_page();
  file_name = path;

  if(data_type == DATA_TYPE_PHRASE_DATA) {
    file_name.append("/pdata.").append(suffix);
  }
  else if(data_type == DATA_TYPE_PHRASE_ADDR) {
    file_name.append("/paddr.").append(suffix);
  }
  else if(data_type == DATA_TYPE_DOCUMENT_DATA) {
    file_name.append("/ddata.").append(suffix);
  }
  else if(data_type == DATA_TYPE_DOCUMENT_ADDR) {
    file_name.append("/daddr.").append(suffix);
  }
  else if(data_type == DATA_TYPE_REGULAR_INDEX) {
    file_name.append("/regindex.").append(suffix);
  }
  else if(data_type == DATA_TYPE_REVERSE_INDEX) {
    file_name.append("/revindex.").append(suffix);
  }
  else if(data_type == DATA_TYPE_DOCUMENT_INFO) {
    file_name.append("/dinfo.").append(suffix);
  }
  else if(data_type == DATA_TYPE_PHRASE_INFO) {
    file_name.append("/pinfo.").append(suffix);
  }
  else if(data_type == DATA_TYPE_REGULAR_INDEX_INFO) {
    file_name.append("/reginfo.").append(suffix);
  }
  else if(data_type == DATA_TYPE_REVERSE_INDEX_INFO) {
    file_name.append("/revinfo.").append(suffix);
  }
  else {
    file_name.append("/unknown.").append(suffix);
  }

  page_info.type       = data_type;
}


std::string FileAccess::get_file_name() {
  return file_name;
}


void FileAccess::set_page_size(unsigned int _page_size) {
  // destroy old page
  clear_page();

  // page_size is rounded by getpagesize()
  if(_page_size % getpagesize() != 0) 
     page_size = getpagesize() * ((_page_size+getpagesize()) / getpagesize()); 
  else page_size = _page_size;
  page_carry = MAX_FILE_SIZE / page_size;

  // reset
  page_ptr = NULL;
}

unsigned int FileAccess::get_page_size() {
  return page_size;
}


void FileAccess::set_shared_memory(SharedMemoryAccess* _shm) {
  shm = _shm;
  set_page_size(shm->get_page_size());
}

SharedMemoryAccess* FileAccess::get_shared_memory() {
  return shm;
}


bool FileAccess::is_directory(std::string path) {
  struct stat st;
  if(stat(path.c_str(), &st) == -1)  return false;
  return S_ISDIR(st.st_mode);
}

bool FileAccess::create_directory(std::string path) {
  if(is_directory(path)) return true;
  return mkdir(path.c_str(), 0777) == 0 ? true : false;
}

size_t FileAccess::get_file_length(const char* filename)
{
    struct stat st;
    if(stat(filename, &st) < 0) {return 0;}
    return st.st_size;
}

size_t FileAccess::get_file_length() {
  return get_file_length(file_name.c_str());
}


bool FileAccess::rename(std::string old_name, std::string new_name) {
  if(::rename(old_name.c_str(), new_name.c_str()) == -1) return false;
  return true;
}

bool FileAccess::rename(std::string new_name) {
  if(::rename(file_name.c_str(), new_name.c_str()) == -1) return false;
  return true;
}


bool FileAccess::remove(std::string name) {
  return ::remove(name.c_str());
}

bool FileAccess::remove() {
  if(!is_file()) return false;
  clear_page();
  remove(file_name);

  return true;
}



bool FileAccess::set_page_info(unsigned short secno, unsigned int pageno, int mode) {
  // int page_offset = page_size * (pageno%page_carry);
  if(page_size == 0) return false;

  page_info.secno = secno;
  page_info.pageno = pageno;
  page_info.mode = mode;

  return true;
}

bool FileAccess::clear_page_info() {
  page_info.secno = DATA_SECTOR_LEFT;
  page_info.pageno = 0;
  page_info.mode = PAGE_NONE;

  return true;
}


FileHandler FileAccess::get_handler(PageInfo p) {
  if(file_name == "") throw AppException(EX_APP_FILE_ACCESS, "no file");

  char f[file_name.length()+20];
  FileHandler h = {p.secno, p.pageno/page_carry, -1};

  for(unsigned int i=0; i<handlers.size(); i++) {
    if(handlers[i].secno == h.secno && handlers[i].fileno == h.fileno) {
      return handlers[i];
    }
  }

  // if(h.fileno == 0) sprintf(f, "%s", file_name.c_str());
  //else              sprintf(f, "%s.%08x.%04x", file_name.c_str(), h.fileno, h.secno);
  sprintf(f, "%s.%08x.%04x", file_name.c_str(), h.fileno, h.secno);

  h.fd = open(f, O_RDWR | O_CREAT, 0666);
  if(h.fd != -1) {
    handlers.push_back(h);
  }

  return h;
}

void FileAccess::clear_handler() {
  for(unsigned int i=0; i<handlers.size(); i++) {
    close(handlers[i].fd); 
  }
  handlers.clear();
}


// load page from file or shared memory
void* FileAccess::load_page(unsigned short secno, unsigned int pageno, int mode) {
  int page_offset = page_size * (pageno%page_carry);
  void* map_ptr = NULL;
  std::string errmes;
  FileHandler h;

  if(page_info.secno == secno && page_info.pageno == (int)pageno && page_info.mode != PAGE_NONE) {
    if(!set_page_info(secno, pageno, mode)) {
      errmes = "Failed to set page info";
      goto load_error;
    }
    if(mode == PAGE_READWRITE) page_info.mode = PAGE_READWRITE;
    return page_ptr;
  }

  save_page();
  if(!set_page_info(secno, pageno, mode))  {
    errmes = "Failed to set page info";
    goto load_error;
  }

  // no shared memory => direct mmap
  if(!shm) {
    return map_page(secno, pageno, mode);
  }

  // cache hit
  page_ptr = shm->load_unit(page_info);
  if(page_ptr)  {
    return page_ptr;
  }

  // mapping from file
  h = get_handler(page_info); 
  if(h.fd == -1) {
    errmes = "File open failed";
    goto load_error;
  }

  map_ptr = mmap(NULL, page_size, PROT_READ, MAP_SHARED, h.fd, page_offset);
  if(map_ptr == MAP_FAILED) {
    errmes = "File map failed";
    goto load_error;
  }
 
  // new unit with locking 
  page_ptr = shm->new_unit(map_ptr, page_info, page_size);
  if(!page_ptr) {
    errmes = "Page load failed";
    goto load_error;
  }

  if(munmap(map_ptr, page_size) == -1) {
    errmes = "Unmap failed";
    goto load_error;
  }

  return page_ptr;

load_error:
  std::cout << errmes << "\n";
  clear_page_info();
  return NULL;
}


void* FileAccess::map_page(unsigned short secno, unsigned int pageno, int mode) {
  int page_offset = page_size * (pageno%page_carry);
  FileHandler h = get_handler(page_info); 

  page_ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, h.fd, page_offset);
  if(page_ptr == MAP_FAILED) {
    return NULL; 
  }

  return page_ptr;
}


// save page to shared memory
bool FileAccess::save_page() {
  if(shm) {
    shm->save_unit(page_info);
  } else {
    if(page_ptr) munmap(page_ptr, page_size);
  }
  clear_page_info();

  return true;
}


// save page to file
bool FileAccess::write_page(void* src, unsigned int size) {
  void* map_ptr = NULL;
  int   page_offset = page_size * (page_info.pageno%page_carry);

  FileHandler h = get_handler(page_info);
  if(h.fd == -1) return false;
  fcntl(h.fd, LOCK_EX);

  map_ptr = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, h.fd, page_offset);
  if(map_ptr == MAP_FAILED) {
    return false;
  }

  memcpy(map_ptr, src, size);

  if(munmap(map_ptr, page_size) == -1) {
    return false;
  }
  fcntl(h.fd, LOCK_UN);

  return true;
}


bool FileAccess::clear_page() { // not save
  bool result = true;

  if(shm) {
    if(page_info.mode == PAGE_NONE) {
      // noop
    } else if(page_info.mode == PAGE_READWRITE) {
      // result = shm->remove_unit(page_info);
      result = shm->unlock(page_info);
    } else {
      result = shm->unlock(page_info);
    }
  } else {
    if(page_ptr) munmap(page_ptr, page_size);
  }

  clear_page_info();
  clear_handler();
  page_ptr = NULL;

  return result;
}


// page add
bool FileAccess::add_page(const void* buf, unsigned short secno, unsigned int pageno) {
  return add_page(buf, secno, pageno, shm ? shm->get_page_size() : page_size);
}

bool FileAccess::add_page(const void* buf, unsigned short secno, unsigned int pageno, unsigned int buf_size) {
  if(buf_size > page_size) return false;

  PageInfo new_page_info = {secno, pageno, PAGE_READWRITE, page_info.type};
  FileHandler h = get_handler(new_page_info);
  if(h.fd == -1) return false;
  lseek(h.fd, 0, L_XTND);

  unsigned int total_write_size = 0;
  if(buf) {
    write(h.fd, buf, buf_size);
    total_write_size = buf_size;
  }

  while(total_write_size < page_size) {
    if(total_write_size+EMPTY_BUFFER_SIZE < page_size) {
      write(h.fd, empty_buffer, EMPTY_BUFFER_SIZE);
      total_write_size = total_write_size + EMPTY_BUFFER_SIZE;
    } else {
      write(h.fd, empty_buffer, page_size-total_write_size);
      total_write_size = page_size;
    }
  }

  // allocate to shared memory
  if(shm) {
    shm->new_unit(buf, new_page_info, buf_size); 
    shm->save_unit(new_page_info);
  }

  return true;
}


void FileAccess::empty_buffer_init() {
  memset(empty_buffer, 0, EMPTY_BUFFER_SIZE);
}



/////////////////////////////////////////////////
// for debug
/////////////////////////////////////////////////
bool FileAccess::test() {
  if(!shm) return false;

  set_page_size(getpagesize()*4);
  shm->init(page_size, 10);

  char buf[page_size];

  // create first page
  std::cout << "create first page\n"; 
  memset(buf, 'a', page_size);
  if(!add_page(buf, 0, 0)) return false;

  std::cout << "create second page\n";
  memset(buf, 'b', page_size);
  add_page(buf, 0, 1);
 
  std::cout << "create third page\n";
  memset(buf, 'c', page_size);
  add_page(buf, 0, 2);

  std::cout << "another sector page\n";
  memset(buf, 'd', page_size);
  add_page(buf, 1, 2);

  remove_with_suffix();
  shm->init(page_size, 10);

  // create huge page
  std::cout << "create huge page\n";
  for(unsigned int i=0; i<10000; i++) {
    memset(buf, 'a' + i%20, page_size);
    add_page(buf, 0, i);
  }

  std::cout << "load loop check(read)\n";
  for(unsigned int i=0; i<10000; i++) { 
    char* ptr = (char*)load_page(0, i%10000, PAGE_READONLY);

    unsigned int idx = rand() % page_size;
    if((unsigned int)ptr[idx] != 'a' + i%20) {
      return false;
    }
    if(!ptr) return false;
  }

  char* ptr = (char*)load_page(0, 333, PAGE_READWRITE);
  memset(ptr, 'z', page_size);
  save_page();

  ptr = (char*)load_page(0, 777, PAGE_READWRITE);
  memset(ptr, 'y', page_size);
  clear_page();


  ptr = (char*)load_page(0, 333, PAGE_READONLY);
  if(ptr[0] != 'z') return false;

  ptr = (char*)load_page(0, 777, PAGE_READONLY);
  if(ptr[0] != 'y') return false;

  std::cout << "clean up\n";
  remove_with_suffix();

  std::cout << "end\n";
  return true;
}



