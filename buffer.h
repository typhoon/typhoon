/*****************************************************************
 *  buffer.h(class Buffer)
 *    brief:
 *     class Buffer provides dynamic memory storage.
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 10:18:46 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *****************************************************************/

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include "common.h"

class Buffer {
  public:
    Buffer();
    Buffer(unsigned int);
    ~Buffer();

    char*  allocate(unsigned int); 
    char*  allocate();
    char*  reallocate(unsigned int, unsigned int);
    char*  reallocate(unsigned int);
    char*  get(unsigned int);
    int    get_size();
    void   resize(unsigned int);

    void   clear(); 
    bool   test();

  private:
    std::vector<char*> data;
    unsigned int       size;
    unsigned int       next;
};

#endif
