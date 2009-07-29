/*****************************************************************
 *  buffer.cc
 *    temporary buffer
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 10:18:46 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *****************************************************************/

#include "buffer.h"

Buffer :: Buffer() {
  data.clear();
  size = getpagesize();
  next = size;
}

Buffer::Buffer(unsigned int _size) {
  data.clear();
  size = _size;
  next = size;
}

Buffer::~Buffer() {
  clear();
}


void Buffer::clear() {
  for(unsigned int i=0; i<data.size(); i++) {
    free(data[i]);
  }
  data.clear();
  next = size;
}

char* Buffer::allocate(unsigned int len) {
  if(len > size) return NULL;

  char* ptr;
  if(next+len > size) {
    ptr = (char*)malloc(size);
    data.push_back(ptr);
    next = len;
  } else {
    ptr = data[data.size()-1]+next;
    next = next + len;
  } 

  return ptr;
}

char* Buffer::allocate() {
  return allocate(size);
}


char* Buffer::get(unsigned int i) {
  if(i >= data.size()) return NULL;
  return data[i];
}


int Buffer::get_size() {
  return next;
}


void Buffer::resize(unsigned int _size) {
  clear();
  size = _size; 
  next = size; 
}

char* Buffer::reallocate(unsigned int _size, unsigned int len) {
  resize(_size);
  return allocate(len);  
}

char* Buffer::reallocate(unsigned int _size) {
  resize(_size);
  return allocate();
}


////////////////////////////////////////////////////////////////
//  for debug
////////////////////////////////////////////////////////////////
bool Buffer::test() {
  size = 100;
  clear();

  char* ptr;
  std::cout << "overflow test\n";
  ptr = allocate(1000);
  if(ptr != NULL) return false;

  std::cout << "pagenate test\n"; 
  ptr = allocate(80);
  if(ptr == NULL || data.size() != 1) return false;

  ptr = allocate(50);
  if(ptr == NULL || data.size() != 2) return false;

  ptr = allocate(20);
  if(ptr == NULL || data.size() != 2) return false;

  std::cout << "realloc test\n"; 
  ptr = reallocate(1000000, 1000000);
  if(ptr == NULL || data.size() != 1) return false;

  ptr = get(100);
  if(ptr != NULL) return false;

  ptr = get(0);
  if(ptr == NULL) return false;

  return true;
}
