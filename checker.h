/*****************************************************************
 *  checker.h
 *    brief:
 *
 *  $Author: imamura $
 *  $Date:: 2009-01-29 17:29:41 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#ifndef __CHECKER_H__
#define __CHECKER_H__

#include <string>
#include <iostream>
#include <vector>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
#include "app_config.h"
#include "data_controller.h"
#include "morph_controller.h"
#include "buffer.h"
#include "indexer.h"
#include "searcher.h"

#include <json.h>

class CheckerConfig : public AppConfig
{
public:
  std::string test_mode;
  unsigned int dump_id;
  std::string  dump_mode;
};


bool get_options(int argc, char* const argv[]);
void test(void);
void dump(void);
void dump_all(void);

#endif // __CHECKER_H__
