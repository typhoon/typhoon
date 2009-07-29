// Definition searcher common objects
#ifndef __SHARED_MEMORY_ACCESS_H__
#define __SHARED_MEMORY_ACCESS_H__

#include <string>
#include <iostream>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "file_access.h"

#define MAX_LOCK_LOOP 100

struct SharedMemoryHeader {
  // bool external_mutex[EXTERNAL_MUTEX_COUNT];
  bool internal_mutex;
  unsigned int empty_block;
  int generation;
  PhraseHeader       p_header;
  DocumentHeader     d_header;
  RegularIndexHeader reg_header;
  ReverseIndexHeader rev_header; 
};


struct SharedMemoryBlock {
  unsigned short sector;
  unsigned int   val;
  int            next; // for hash
  bool           update_flag;
  bool           lock_flag;
  unsigned int   refer;
  int   generation;
};


class SharedMemoryAccess {
public:
  SharedMemoryAccess();
  SharedMemoryAccess(unsigned int, unsigned int);
  SharedMemoryAccess(unsigned int, unsigned int, bool);
  ~SharedMemoryAccess();

  void* load_unit(struct PageInfo&);
  bool  save_unit(struct PageInfo&);
  void* new_unit(const void*, struct PageInfo&, unsigned int);
  bool  remove_unit(struct PageInfo&);

  bool lock(struct PageInfo&);
  bool unlock(struct PageInfo&);
  bool lock_all();
  bool unlock_all();

  bool save();
  bool check();
  bool test();
  bool dump();

  bool init();
  bool init(unsigned int, unsigned int);
  bool setup();
  bool setup(unsigned int, unsigned int);
  bool release();

  unsigned int get_page_size();
  unsigned int get_block_size();

  void set_size(unsigned int, unsigned int);
  void set_path(std::string&);

  bool save_header();
  bool load_header();
  SharedMemoryHeader* get_header();

  void next_generation();

  clock_t c1, c2, c3;  // for benchmark
  int x1, x2, x3;

private:
  std::string path;
  
  unsigned int block_size;
  unsigned int unit_size;
  unsigned int hash_size;

  class FileAccess*    shm_file;
  void*                shm;

  struct SharedMemoryHeader*  shm_header;
  struct SharedMemoryBlock*   shm_block;
  int*                        shm_hash_table;
  char*                       shm_data;

  struct SharedMemoryInfo* block_info;

  int  find_hash_list(struct PageInfo&);
  bool remove_hash_list(struct PageInfo&, unsigned int);
  bool add_hash_list(struct PageInfo&, unsigned int);
  bool write_unit(unsigned int);

  bool internal_lock();
  bool internal_unlock();

  void dump_block(unsigned int);
};

#endif // __SHARED_MEMORY_ACCESS_H__
