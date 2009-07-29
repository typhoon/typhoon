/*****************************************************************
 *  exception.cc
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-24 16:15:55 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/
#include "common.h"
#include "exception.h"

AppException::AppException(int _type, std::string _message) {
  type = _type;
  message = _message;
}
  

std::string AppException::what() {
  switch(type) {
  case EX_APP_INDEXER:
    message = message + "(at indexer)";
    break;
  case EX_APP_SEARCHER:
    message = message + "(at searcher)";
    break;
  case EX_APP_SHARED_MEMORY:
    message = message + "(at shared memory)";
    break;
  case EX_APP_FILE_ACCESS:
    message = message + "(at file access)";
    break;
  case EX_APP_DOCUMENT:
    message = message + "(at document)";
    break;
  case EX_APP_PHRASE:
    message = message + "(at phrase)";
    break;
  default:
    message = message + "(unknown)";
    break;
  };
  
  return message; 
}


