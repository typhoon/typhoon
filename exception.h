/*****************************************************************
 *  exception.h
 *    brief:
 *      Exception class. AppException is base-class of exception
 *      which generated from search engine.
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-05 15:38:11 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

class AppException {
public:
  AppException(int type, std::string message);
  std::string what();

private:
  int type;
  std::string message; 
};

#endif
