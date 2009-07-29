/*****************************************************************
 *  shared_memory_access.cc
 *    brief: 
 *      operation to shared memory (unused)
 *      1. load: get memory block from shared memory
 *      2. save: save memory block(but not saved immediately) 
 *      3. remove: remove memory block(removed only from shared memory)
 *      4. lock: internal/external/page lock
 *      5. new:  create memory block from pointer(overwrited block is saved to file)
 ****************************************************************/

#include "shared_memory_access.h"
#include "exception.h"

SharedMemoryAccess::SharedMemoryAccess() {
  unit_size = getpagesize();
  block_size = 1;
  hash_size  = 1;

  shm_file = new FileAccess();
  shm = NULL;
  shm_header = NULL;
  shm_block = NULL;
  shm_data = NULL;

  c1 = c2 = c3 = 0;
  x1 = x2 = 0;

  srand(time(NULL));
}

SharedMemoryAccess::SharedMemoryAccess(unsigned int _unit_size, unsigned int _block_size) {
  unit_size = _unit_size;
  block_size = _block_size;
  hash_size = block_size > 1 ? block_size-1 : 1;

  shm_file = new FileAccess();
  shm = NULL;
  shm_header = NULL;
  shm_block = NULL;
  shm_data = NULL;

  c1 = c2 = c3 = 0;
  x1 = x2 = 0;

  srand(time(NULL));
}


SharedMemoryAccess::~SharedMemoryAccess() {
  release();
  delete shm_file;
}


unsigned int SharedMemoryAccess::get_page_size() {
  return unit_size;
}

unsigned int SharedMemoryAccess::get_block_size() {
  return block_size;
}

void SharedMemoryAccess::set_size(unsigned int _unit_size, unsigned int _block_size) {
  unit_size = _unit_size;
  block_size = _block_size;
  hash_size = (block_size > 1) ? (block_size-1) : block_size;
}


void SharedMemoryAccess::set_path(std::string& _path) {
  release();
  path = _path;
  shm_file->set_file_name(path + "/shm.dat");
}


bool SharedMemoryAccess::init(unsigned int _unit_size, unsigned int _block_size) {
  set_size(_unit_size, _block_size);
  return init();
}

bool SharedMemoryAccess::init() {
  if(shm != NULL) release(); // discard old data

  unsigned int allocate_size = sizeof(SharedMemoryHeader) + sizeof(SharedMemoryBlock)*block_size + 
                               sizeof(int)*block_size + unit_size*block_size;

  // allocate
  shm_file->remove_with_suffix();
  shm_file->set_page_size(allocate_size);
  shm_file->add_page(NULL, 0, 0);
  shm = shm_file->load_page(0, 0, PAGE_READWRITE);
  if(!shm) goto allocate_error;

  // set header
  shm_header = (SharedMemoryHeader*)shm;
  shm_header->internal_mutex = false;
  shm_header->empty_block = block_size;
  shm_header->generation = 0;
  shm_block  = (SharedMemoryBlock*)(shm_header+1);

  for(unsigned int i=0; i<block_size; i++) { 
    shm_block[i].sector = 0, shm_block[i].val = 0, shm_block[i].next = -1, shm_block[i].refer = 0;
    shm_block[i].update_flag = false, shm_block[i].lock_flag = false, shm_block[i].generation = 0;
  }

  shm_hash_table = (int*)(shm_block+block_size);
  for(unsigned int i=0; i<block_size; i++)  shm_hash_table[i] = -1;

  shm_data   = (char*)(shm_hash_table + block_size);
  memset(shm_data, 0, (unit_size * block_size));
  return true;

allocate_error:
  release();
  return false;
}



// There must be already allocated files
bool SharedMemoryAccess::setup(unsigned int _unit_size, unsigned int _block_size) {
  set_size(_unit_size, _block_size);
  return setup();
}

bool SharedMemoryAccess::setup() {
  if(shm != NULL) release(); // discard old data

  unsigned int allocate_size = sizeof(SharedMemoryHeader) + sizeof(SharedMemoryBlock)*block_size + 
                               sizeof(int)*block_size + unit_size*block_size;

  shm_file->set_page_size(allocate_size);
  shm = shm_file->load_page(0, 0, PAGE_READWRITE);
  if(!shm) goto allocate_error;

  shm_header = (SharedMemoryHeader*)shm;
  shm_header->internal_mutex = false;
  shm_header->generation = 0;
  shm_block  = (SharedMemoryBlock*)(shm_header+1);
  for(unsigned int i=0; i<block_size; i++) { 
    shm_block[i].lock_flag = false;
  }
  shm_hash_table = (int*)(shm_block+block_size);
  shm_data   = (char*)(shm_hash_table + block_size);
  unlock_all();
  return true;

allocate_error:
  release();
  return false;
}


bool SharedMemoryAccess::release() {
  shm_file->clear_page();

  shm = NULL;
  shm_header = NULL;
  shm_block = NULL;
  shm_data = NULL;

  return true;
}



void* SharedMemoryAccess::new_unit(const void* src, struct PageInfo& info, unsigned int src_size) {
  int block = -1;

  void* ptr = load_unit(info);

  // page not exists
  if(!internal_lock()) return NULL;

  if(ptr) {  // page already exists
    if(info.mode == PAGE_READWRITE) {
      memset(ptr, 0, unit_size);
      if(src) memcpy(ptr, src, src_size);
    }
    internal_unlock();
    return ptr;
  }

  if(shm_header->empty_block > 0) {
    block = block_size - shm_header->empty_block;
    shm_header->empty_block--;
  } else {
    unsigned int i=0;
    for(;;) {
      int tmp_block = ((unsigned int)((rand() << 16) | (rand() & 0x0000FFFF))) % block_size;
      if(shm_block[tmp_block].lock_flag || shm_block[tmp_block].refer != 0) continue;

      if(i==0 || abs(shm_header->generation-shm_block[tmp_block].generation) > abs(shm_header->generation-shm_block[block].generation)) {
        block = tmp_block;
      }
      i++;
      if(i>20) break;
    }

    if(!write_unit(block))  {
      internal_unlock();
      return NULL;
    }

    PageInfo remove_page_info = {shm_block[block].sector, VAL_TO_FILE_PAGE(shm_block[block].val), PAGE_READWRITE, 
                                 VAL_TO_FILE_TYPE(shm_block[block].val)};
    remove_hash_list(remove_page_info, block);
  }

  //std::cout << "====\n";
  //dump_block(block);
  add_hash_list(info, block);
  shm_block[block].update_flag = (info.mode == PAGE_READWRITE ? true : false);
  shm_block[block].lock_flag = (info.mode == PAGE_READWRITE ? true : false);
  shm_block[block].refer = 1;
  shm_block[block].generation = shm_header->generation;
  memset(shm_data+unit_size*block, 0, unit_size);
  if(src) memcpy(shm_data+unit_size*block, src, src_size);
  //dump_block(block);
  //std::cout << "----\n";

  internal_unlock();
  return shm_data+unit_size*block;
}


void* SharedMemoryAccess::load_unit(struct PageInfo& info) {
  int block = -1;
  int loop_cnt = 0;
  while(1) {
    if(!internal_lock()) return NULL;
    block = find_hash_list(info);
    if(block == -1) break;
    if(!shm_block[block].lock_flag) {
      if(info.mode == PAGE_READWRITE) {
        shm_block[block].lock_flag = true;
        shm_block[block].update_flag = true;
      }
      shm_block[block].refer++;
      shm_block[block].generation = shm_header->generation;
      break;
    }
    internal_unlock();

    usleep(SBM_WAIT_DURATION); // wait page lock
    loop_cnt ++;
    if(info.mode == PAGE_READONLY && loop_cnt > MAX_LOCK_LOOP) {
      return NULL;
    } 
  }

  internal_unlock();
  return (block == -1) ? NULL : (shm_data + unit_size*block);
}


bool SharedMemoryAccess::save_unit(struct PageInfo& info) {
  if(!internal_lock()) return false;

  int block = find_hash_list(info);
  if(block != -1) {
    shm_block[block].update_flag = shm_block[block].update_flag || (info.mode == PAGE_READWRITE ? true : false); 
    shm_block[block].lock_flag = false;
    if(shm_block[block].refer > 0) shm_block[block].refer--;
  }
  internal_unlock(); 

  return (block == -1) ? false : true;
}


bool SharedMemoryAccess::remove_unit(struct PageInfo& info) {
  if(!internal_lock()) return false;

  int block = find_hash_list(info);
  if(block != -1) {
    remove_hash_list(info, block);
    shm_block[block].lock_flag = false;  
    shm_block[block].refer = 0; 
    shm_block[block].update_flag = false;  
    shm_block[block].generation = 0;
  }

  internal_unlock();
  return (block == -1) ? false : true;
}



bool SharedMemoryAccess::lock(struct PageInfo& info) {
  if(!internal_lock()) return false;
 
  int block = find_hash_list(info);
  if(block != -1) {
    shm_block[block].lock_flag = true;
    shm_block[block].refer++;
  }
  

  internal_unlock();
  return (block == -1) ? false : true;
}

bool SharedMemoryAccess::unlock(struct PageInfo& info) {
  if(!internal_lock()) return false;

  int block = find_hash_list(info);
  if(block != -1) {
    shm_block[block].lock_flag = false;
    if(shm_block[block].refer > 0) shm_block[block].refer--;
  }

  internal_unlock();
  return (block == -1) ? false : true;
}


bool SharedMemoryAccess::lock_all() {  // force lock
  if(!shm) return true;

  unsigned int wait_count = 0;
  while(1) {
    if(!shm_header->internal_mutex) break;
    usleep(SBM_WAIT_DURATION);
    wait_count++;
    if(wait_count > MAX_LOCK_LOOP) break;
  }
  shm_header->internal_mutex = true; 

  for(unsigned int i=0; i<block_size; i++) {
    wait_count = 0;
    while(1) {
      if(!shm_block[i].lock_flag) break;
      usleep(SBM_WAIT_DURATION);
      wait_count++;
      if(wait_count > MAX_LOCK_LOOP) break;
    }
    shm_block[i].lock_flag = true;
    shm_block[i].refer++;
  }

  return true;
}

bool SharedMemoryAccess::unlock_all() { // force unlock
  if(!shm) return false;
  internal_unlock();

  for(unsigned int i=0; i<block_size; i++) {
    shm_block[i].lock_flag = false;
    if(shm_block[i].refer > 0) shm_block[i].refer--;
  }
  return true;
}



int SharedMemoryAccess::find_hash_list(struct PageInfo& info) {
  unsigned int page_val = info.type | info.pageno;
  unsigned int hash_val = page_val % hash_size;  // block_size is not good for hash size.


  if(shm_hash_table[hash_val] == -1) return -1;
  int block = shm_hash_table[hash_val];
  while(block != -1) {
    if(shm_block[block].sector == info.secno && shm_block[block].val == page_val) return block;
    block = shm_block[block].next;
  }

  return -1;
}

bool SharedMemoryAccess::remove_hash_list(struct PageInfo& info, unsigned int block) {
  unsigned int page_val = info.type | info.pageno;
  unsigned int hash_val = page_val % hash_size;  // block_size is not good for hash size.

  int next = shm_hash_table[hash_val];
  if(next == -1)           return false; // no match block
  else if(next == (int)block)   {
    shm_hash_table[hash_val] = shm_block[block].next;
    return true;
  }

  unsigned int prev;
  while(next != (int)block) {
    if(next == -1) return false; // no match block

    prev = next;
    next = shm_block[prev].next;
  }
  
  shm_block[prev].next = shm_block[next].next;
  return true;
}

bool SharedMemoryAccess::add_hash_list(struct PageInfo& info, unsigned int block) {
  unsigned int page_val = info.type | info.pageno;
  unsigned int hash_val = page_val % hash_size;  // block_size is not good for hash size.

  int next = shm_hash_table[hash_val];
  if(next == -1) {
    shm_hash_table[hash_val] = block;
  } else {
    int prev = 0;
    while(next != -1) {
      prev = next;
      next = shm_block[prev].next;
    }
    shm_block[prev].next = block; 
  }

  shm_block[block].sector = info.secno;
  shm_block[block].val = page_val;
  shm_block[block].next = -1; 

  return true;
}


bool SharedMemoryAccess::write_unit(unsigned int block) {
  if(!shm_block[block].update_flag) {
    return true;
  }

  FileAccess f;
  f.set_file_name(path, VAL_TO_FILE_TYPE(shm_block[block].val), "dat");
  f.set_page_size(unit_size);
  f.set_page_info(shm_block[block].sector, VAL_TO_FILE_PAGE(shm_block[block].val), PAGE_READWRITE); 
  if(!f.write_page(shm_data+block*unit_size, unit_size)) {
    return false;
  }

  shm_block[block].update_flag = false;
  return true;
}


bool SharedMemoryAccess::internal_lock() {
  while(shm_header->internal_mutex) {
    usleep(SBM_WAIT_DURATION);
  }
  shm_header->internal_mutex = true; 

  return true;
}


bool SharedMemoryAccess::internal_unlock() {
  shm_header->internal_mutex = false;
  return true;  
}



SharedMemoryHeader* SharedMemoryAccess::get_header() {
  return shm_header;
}

void SharedMemoryAccess::next_generation() {
  shm_header->generation = (shm_header->generation + 1) % MAX_GENERATION;
}



/////////////////////////////////////////////////////////////////////////
//  for debug
/////////////////////////////////////////////////////////////////////////
bool SharedMemoryAccess::test() {
  void* buffer = NULL; 
  void* ptr = NULL;
  bool  result = true;

  release();

  try {
    std::cout << "initialize test...\n";
    if(!init(4096, 100)) throw AppException(EX_APP_SHARED_MEMORY, "failed");

    std::cout << "release test...\n";
    if(!release()) throw AppException(EX_APP_SHARED_MEMORY, "failed");

    std::cout << "re-initialize test...\n";
    if(!init(4096, 200)) throw AppException(EX_APP_SHARED_MEMORY, "failed");

    std::cout << "setup test...\n";
    if(!setup()) throw AppException(EX_APP_SHARED_MEMORY, "failed");

    buffer = malloc(unit_size);
    std::cout << "read/write unit...\n";
    memset(buffer, 'a', unit_size);
    PageInfo info = {0, 0, PAGE_READWRITE, DATA_TYPE_PHRASE_ADDR};
    new_unit(buffer, info, unit_size); 
    save_unit(info);

    ptr = load_unit(info);
    if(!ptr || ((char*)ptr)[0] != 'a') throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);

    std::cout << "readonly unit...\n";
    memset(buffer, 'b', unit_size);
    info.mode = PAGE_READONLY, info.pageno = 1;
    new_unit(buffer, info, unit_size);
    save_unit(info);

    ptr = load_unit(info);
    if(!ptr || ((char*)ptr)[0] != 'b') throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);

    std::cout << "sector change...\n";
    memset(buffer, 'c', unit_size);
    info.secno = 1;
    new_unit(buffer, info, unit_size);
    save_unit(info);

    ptr = load_unit(info);
    if(!ptr || ((char*)ptr)[0] != 'c') throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);

    info.secno = 0;
    ptr = load_unit(info);
    if(!ptr || ((char*)ptr)[0] != 'b') throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);


    std::cout << "same unit...\n";
    info.mode = PAGE_READWRITE;
    memset(buffer, 'd', unit_size);
    new_unit(buffer, info, unit_size);
    save_unit(info);

    ptr = load_unit(info);
    if(!ptr || ((char*)ptr)[0] != 'd') throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);


    std::cout << "not exists ...\n";
    info.pageno = 2;
    ptr = load_unit(info);
    if(ptr) throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);

/*
    std::cout << "huge data test...\n";
    memset(buffer, 'e', unit_size);
    info.secno = 2;
    for(unsigned int i=0; i<1000; i++) {
      info.pageno = i;
      info.mode = PAGE_READONLY;
      new_unit(buffer, info, unit_size);
      ptr = load_unit(info);
      if(!ptr || ((char*)ptr)[0] != 'e') throw AppException(EX_APP_SHARED_MEMORY, "failed");
      save_unit(info);
    }
*/

    std::cout << "remove data test...\n";
    memset(buffer, 'f', unit_size);
    info.secno = 3;
    new_unit(buffer, info, unit_size);
    save_unit(info);

    remove_unit(info);
    ptr = load_unit(info);
    if(ptr) throw AppException(EX_APP_SHARED_MEMORY, "failed");
    save_unit(info);

/*
    std::cout << "lock test...\n";
    info.secno = 4;
    new_unit(buffer, info, unit_size);
    ptr = load_unit(info);
    try {
      load_unit(info);
      result = false;
    } catch(AppException e) {
      result = true;
    }
    save_unit(info);
*/
  } catch(AppException e) {
    std::cout << e.what() << "\n";
    dump();
    result = false;
  }

  if(buffer) free(buffer);

  return result;
}


bool SharedMemoryAccess::dump() {
  if(!shm_data) return false;

  std::cout << "shared memory status: \n";
  std::cout << "empty blocks: " << shm_header->empty_block << "\n";
  for(unsigned int i=0; i<block_size-shm_header->empty_block; i++) {
    dump_block(i);
  }

  std::cout << "---\n";
  return true;
}

void SharedMemoryAccess::dump_block(unsigned int i) {
  std::cout << i << ":";
  std::cout << (shm_block[i].val % hash_size) << "\t";

  if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_PHRASE_DATA) std::cout << "[PDATA]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_PHRASE_ADDR) std::cout << "[PADDR]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_DOCUMENT_DATA) std::cout << "[DDATA]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_DOCUMENT_ADDR) std::cout << "[DADDR]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_REGULAR_INDEX) std::cout << "[REGINDEX]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_REVERSE_INDEX) std::cout << "[REVINDEX]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_DOCUMENT_INFO) std::cout << "[DINFO]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_PHRASE_INFO) std::cout << "[PINFO]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_REGULAR_INDEX_INFO) std::cout << "[REGINFO]";
  else if(VAL_TO_FILE_TYPE(shm_block[i].val) == DATA_TYPE_REVERSE_INDEX_INFO) std::cout << "[REVINFO]";
  else std::cout << "[UNKNOWN]";

  std::cout << VAL_TO_FILE_PAGE(shm_block[i].val) << "(sector " << shm_block[i].sector << ")";

  if(shm_block[i].next != -1) std::cout << "->next: " << shm_block[i].next;
  std::cout << "\t";
  std::cout << (shm_block[i].update_flag ? "updated" : "-") << "\t";
  std::cout << (shm_block[i].lock_flag ? "locked" : "-") << "\t";
  std::cout << shm_block[i].generation << "\t";
  std::cout << shm_block[i].refer << "\n";
}


bool SharedMemoryAccess::check() {
  if(!shm) return false;
  unsigned int fill_count = 0;
  std::cout << "---\n";
  for(unsigned int i=0; i<hash_size; i++) {
    std::cout << "[" << i << "]";
    int block = shm_hash_table[i];
    while(block != -1) {
      fill_count++;
      std::cout << "->" << block;
      if(shm_block[block].val % hash_size != i) {
        dump();
        return false;
      }
      block = shm_block[block].next;
    }
    std::cout << "\n";
  }

  if(fill_count + shm_header->empty_block != block_size) {
    dump();
    return false;
  }
  return true;
}

