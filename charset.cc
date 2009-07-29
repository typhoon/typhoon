/*******************************************************
 * charset.cc
 *   Character conversion functions.
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-04 12:36:30 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ******************************************************/
#include "common.h"
#include "charset.h"
#include <unistd.h>


std::string CharsetController::to_half_lower(const char* src) {
  unsigned int srclen = strlen(src);
  char* dst = new char[srclen+1];
  unsigned int i = 0, j = 0;

  while(i < srclen) {
    CharInfo info  = get_char_info(src+i, srclen-i); 
    char lower_char = to_half_lower_char(src+i, info);

    if(lower_char == '\0') {
      strncpy(dst+j, src+i, info.byte);
      i += info.byte;
      j += info.byte;
    } else {
      dst[j] = lower_char;
      i += info.byte;
      j++;
    }
  }

  dst[j] = '\0';
  std::string ret = dst;

  delete[] dst;

  return ret;
}


void CharsetController::set_char_info(CharInfo& info, int byte, int ctrl, int type, int err) {
  info.byte = byte;
  info.ctrl = ctrl;
  info.type = type;
  info.err  = err;
}


#ifdef CHARSET_JA_JP_UTF8
char CharsetController::to_half_lower_char(const char* src, CharInfo info) {
  if(info.err != ERR_NOCHARERR) {
    return '\0';
  } else if(info.byte == 1 && !((unsigned char)src[0] & 0x80)) {
    return tolower(src[0]);
  } else if(info.byte == 3) {
    unsigned char byte1 = (unsigned char)src[0];
    unsigned char byte2 = (unsigned char)src[1];
    unsigned char byte3 = (unsigned char)src[2];

    if(byte1 == 0xE2 && byte2 == 0x80 && (byte3 == 0x9c || byte3 == 0x9d)) {
          return '"';
    } else if(byte1 == 0xE3 && byte2 == 0x80 && byte3 == 0x80) {
          return ' ';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x8C) {
          return ',';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x8E) {
          return '.';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x9A) {
          return ':';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x9B) {
          return ';';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x9F) {
          return '?';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x81) {
          return '!';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0xA0) {
          return '@';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x88) {
          return '(';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0x89) {
          return ')';
    } else if(byte1 == 0xE3 && byte2 == 0x80 && byte3 == 0x94) {
          return '[';
    } else if(byte1 == 0xE3 && byte2 == 0x80 && byte3 == 0x95) {
          return ']';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0xBB) {
          return '[';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 == 0xBD) {
          return ']';
    } else if(byte1 == 0xEF && byte2 == 0xBD && byte3 == 0x9B) {
          return '{';
    } else if(byte1 == 0xEF && byte2 == 0xBD && byte3 == 0x9D) {
          return '}';
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 >= 0x90 && byte3 <= 0x99) {
          return '0' + byte2 - 0x90;
    } else if(byte1 == 0xEF && byte2 == 0xBC && byte3 >= 0xA1 && byte3 <= 0xBA) {
          return 'a' + byte2 - 0xA1;
    } else if(byte1 == 0xEF && byte2 == 0xBD && byte3 >= 0x81 && byte3 <= 0x9A) {
          return 'a' + byte2 - 0x81;
    }
  }

  return '\0';
}

CharInfo CharsetController::get_char_info(const char* text,int text_size)
{
  CharInfo info = {1, 0, 0, ERR_NOCHARERR};

  if(!((unsigned char)text[0] & 0x80)) {
    if((unsigned char)text[0] <= 0x20) {
      set_char_info(info,1, 1, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,1, 0, 0, ERR_NOCHARERR);
    }
  } else if(((unsigned char)text[0] & 0xE0) == 0xC0) {
    if(text_size <= 1) {
      set_char_info(info,text_size, 0, 0, ERR_CHARERR);
    } else if(((unsigned char)text[1] & 0xC0) == 0x80) {
      set_char_info(info,2, 0, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,2, 0, 0, ERR_CHARERR);
    }
  } else if(((unsigned char)text[0] & 0xF0) == 0xE0) {
    if(text_size <= 2) {
      set_char_info(info,text_size, 0, 0, ERR_CHARERR);
    } else if(((unsigned char)text[1] & 0xC0) == 0x80 && ((unsigned char)text[2] & 0xC0) == 0x80) {
      set_char_info(info,3, 0, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,3, 0, 0, ERR_CHARERR);
    }
  } else if(((unsigned char)text[0] & 0xF8) == 0xF0) {
    if(text_size <= 3) {
      set_char_info(info,text_size, 0, 0, ERR_CHARERR);
    } else if(((unsigned char)text[1] & 0xC0) == 0x80 && ((unsigned char)text[2] & 0xC0) == 0x80 &&
              ((unsigned char)text[3] & 0xC0) == 0x80) {
      set_char_info(info,4, 0, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,4, 0, 0, ERR_CHARERR);
    }
  } else if(((unsigned char)text[0] & 0xFC) == 0xF8) {
    if(text_size <= 4) {
      set_char_info(info,text_size, 0, 0, ERR_CHARERR);
    } else if(((unsigned char)text[1] & 0xC0) == 0x80 && ((unsigned char)text[2] & 0xC0) == 0x80 &&
              ((unsigned char)text[3] & 0xC0) == 0x80 && ((unsigned char)text[4] & 0xC0) == 0x80) {
      set_char_info(info,5, 0, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,5, 0, 0, ERR_CHARERR);
    }
  } else if(((unsigned char)text[0] & 0xFE) == 0xFC) {
    if(text_size <= 5) {
      set_char_info(info,text_size, 0, 0, ERR_CHARERR);
    } else if(((unsigned char)text[1] & 0xC0) == 0x80 && ((unsigned char)text[2] & 0xC0) == 0x80 &&
              ((unsigned char)text[3] & 0xC0) == 0x80 && ((unsigned char)text[4] & 0xC0) == 0x80 && 
              ((unsigned char)text[5] & 0xC0) == 0x80) {
      set_char_info(info,6, 0, 0, ERR_NOCHARERR);
    } else {
      set_char_info(info,6, 0, 0, ERR_CHARERR);
    }
  } else {
    set_char_info(info, 1, 0, 0, ERR_CHARERR);
  } 

  return info;
}

#endif




#ifdef CHARSET_JA_JP_EUCJP
char CharsetController::to_half_lower_char(const char* src, CharInfo info) {
  if(info.err != ERR_NOCHARERR) {
    return '\0';
  } else if(!((unsigned char)src[0] & 0x80)) {
    return tolower(src[0]);
  } else if((unsigned char)src[0] == 0xA3) {
    unsigned char byte = (unsigned char)src[1];
    if(byte >= 0xC1 && byte <= 0xDA) {
       return 'a' + byte - 0xC1;
    } else if(byte >= 0xE1 && byte <= 0xFA) {
       return 'a' + byte - 0xE1;
    } else if(byte >= 0xB0 && byte <= 0xB9) {
       return '0' + byte - 0xB0;
    }
  }

  return '\0';
}


CharInfo CharsetController::get_char_info(const char* text,int text_size)
{
  CharInfo info = {1, 0, 0, ERR_NOCHARERR};

  if(!((unsigned char)text[0] & 0x80)) {
    info.byte = 1;
    if((unsigned char)text[0] <= 0x20) {info.ctrl = 1;}
    else {info.ctrl = 0;}
    info.type = 0;
  }
  ///////////////////////////////////////////////////////////////////////////
  // Codeset1 JIS X 0208-1990
  // first  0xA0-0xFF ? 0xA1-0xFE
  // second 0xA0-0xFF ? 0xA1-0xFE
  else if ((unsigned char)text[0] >= 0xA1 && (unsigned char)text[0] <= 0xFE) {
    if ( 1 < text_size && (unsigned char)text[1] >= 0xA1 && (unsigned char)text[1] <= 0xFE) {
      info.byte = 2;
      info.ctrl = 0;
      info.type = 1;
    } else {
      info.err = ERR_CHARERR;
    }
  }
  //////////////////////////////////////////////////////////////////////////
  // Codeset2 JIS X 0210 kana
  // first  0x8E
  // second 0xA0-0xFF  ? 0xA1-0xDF
  else if ((unsigned char)text[0] == 0x8E) {
    if (1 < text_size && (unsigned char)text[1] >= 0xA1 && (unsigned char)text[1] <= 0xDF) {
      info.byte=2;
      info.ctrl=0;
      info.type=2;
    } else {
      info.err = ERR_CHARERR;
    }
  }      
  //////////////////////////////////////////////////////////////////////////////
  // Codeset3 JIS X 0212-1990
  // first  0x8F
  // second 0xA0-0xFF ? 0xA1-0xFE
  // third  0xA0-0xFF ? 0xA1-0xFE
  else if ((unsigned char)text[0] == 0x8F) {
      if (1 < text_size && (unsigned char)text[1] >= 0xA1 && (unsigned char)text[1] <= 0xFE) {
        if(2 < text_size && (unsigned char)text[2] >= 0xA1 &&(unsigned char)text[2] <= 0xFE) {
          info.byte=3;
          info.ctrl=0;
          info.type=3;
        }
        else {
          info.err = ERR_CHARERR;
        }
      } else {
        info.err = ERR_CHARERR;
      }
  }
  else {
    info.err = ERR_CHARERR;
  }

  return info;
}

#endif




