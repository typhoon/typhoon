/***************************************************************
 *  charset.h
 *    Character set conversion.
 *    $Author: imamura $
 *    $Date:: 2009-03-04 14:35:03 +0900#$
 *
 *    Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ***************************************************************/

#ifndef _CHARSET_H_
#define _CHARSET_H_

#include <list>
#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>
#include <ctype.h>

#define ERR_NOCHARERR   0
#define ERR_CHARERR     1
#define ERR_CHARSETERR  2


struct CharInfo {
    int byte;
    int ctrl;
    int type;
    int err;
};


class CharsetController {
public:
  static std::string to_half_lower(const char*);
  static char to_half_lower_char(const char* src, CharInfo info);
  static CharInfo    get_char_info(const char*, int);
  static void set_char_info(CharInfo&, int, int, int, int);

private:

};

#endif
