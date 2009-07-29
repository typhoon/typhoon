/******************************************************************
 * @file data_controller.cc
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 12:37:18 +0900#$
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *******************************************************************/

#ifndef DATA_CONTROLLER_H 
#define DATA_CONTROLLER_H


#include "common.h"
#include "buffer.h"
#include "phrase_controller.h"
#include "document_controller.h"
#include "regular_index_controller.h"
#include "reverse_index_controller.h"
#include "shared_memory_access.h"


class DataController {
public:
  DataController(); 
  ~DataController(); 

  PhraseController         phrase;
  PhraseDataController     phrase_data;
  DocumentController       document;
  DocumentDataController   document_data;
  ReverseIndexController   reverse_index;
  RegularIndexController   regular_index;

  bool setup(std::string, SharedMemoryAccess*);
  bool init(std::string, SharedMemoryAccess*);
  bool init();
  bool save();
  bool reset();
  bool finish();
  void set_link();
  bool set_sample(unsigned char);

  bool test();
  void dump(std::string);

private:
};

#endif
