/*****************************************************************
 *  morph_controller.h
 *    brief:
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-14 09:38:07 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#ifndef __MORPH_H__
#define __MORPH_H__

#include <stdio.h>
#include <string>
#include <vector>
#include "common.h"

#define INPUT_LINE_BUFFER_SIZE 1024
#define INPUT_LINE_BUFFER_MAX  65536


typedef std::vector<struct MorphemeData> MORPH_DATA_SET;

class MorphController {
  public:
    MorphController();
    ~MorphController();

    unsigned int get_morph_data(const char*, MORPH_DATA_SET&);
    unsigned int get_search_phrases(const char*, WORD_SET&, unsigned int);

    bool test(void);
  private:
    void*  tagger;
    bool   word_check(MorphemeData&);
};

struct MorphemeData {
  bool        index;
  bool        ignore;
  bool        before_terminate;
  bool        after_terminate;
  bool        multibyte;
  bool        valid;

  const char*  surface;
  unsigned int length;
};


#endif
