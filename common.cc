/*****************************************************************************
 *  common.h
 *    common functions.
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 11:46:34 +0900#$
 *
 *  Copyright (C) 2008- Drecom Co.,Ltd. All Rights Reserved.
 *****************************************************************************/

#include <time.h>

#include "common.h"
#include "morph_controller.h"

bool g_debug = false;

// create daemon process.
int daemonize()
{
  pid_t pid;
  if((pid = fork()) != 0) {
    exit(0);
  }

  // closing I/O
  for(int i=0; i<3; i++) {
    close(i);
  }

  // tty separate
  setsid();

  // avoid unmount error
  if((chdir("/")) != 0) {
    perror("chdir");
    exit(1);
  }

  umask(0);
  return getpid();
}


// string split-join
unsigned int chomp(std::string& str) {
  unsigned int i=str.length();
  while(i>0) {
    if(str[i-1] != '\r' && str[i-1] != '\n') break;
    i--;
  }  

  str.erase(i);
  return str.length();
}

WORD_SET split(const std::string& str, const std::string& delim) {
  WORD_SET words;

  unsigned int length = 0;
  unsigned int i=0;
  for(; i<str.size(); i++) {
    if(strncmp(str.c_str()+i, delim.c_str(), delim.size()) == 0) {
      length==0 ? words.push_back("") : words.push_back(std::string(str, i-length, length));
      i += delim.size() - 1;
      length = 0;
    } else {
      length++;
    }
  }
  length==0 ? words.push_back("") : words.push_back(std::string(str, i-length, length));

  return words;
}

std::string join(const WORD_SET& words, const std::string& delim) {
  std::string str = "";

  for(unsigned int i=0; i<words.size(); i++) {
    i>0 ? (str += delim + words[i]) : (str += words[i]);
  }

  return str;
}



// logging
std::string log_level_string(unsigned int log_level) {
  switch(log_level) {
  case LOG_LEVEL_DEBUG:
    return "[debug]";
  case LOG_LEVEL_INFO:
    return "[info]";
  case LOG_LEVEL_WARNING:
    return "[warning]";
  case LOG_LEVEL_ERROR:
    return "[error]";
  case LOG_LEVEL_FATAL:
    return "[fatal]";
  default:
    return "[default]";
  }

  return "";
}

void write_log(unsigned int log_level, std::string message, std::string filename) {
  time_t t;
  struct tm *tm;
  char s[50];
  time(&t);
  tm = localtime(&t);
  sprintf(s, "%04d-%02d-%02d %02d:%02d:%02d", tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);

  if(filename == "") {
    std::cout << s << " " << log_level_string(log_level) << " " << message << "\n";
  } else {
    std::ofstream ofs;
    ofs.open(filename.c_str(), std::ios::app);
    if(!ofs) std::cerr << s << ": log file open failed\n";
    else     ofs << s << " " << log_level_string(log_level) << " " << message << "\n";
    ofs.close();
  }
}



// JSON conversion
std::string get_attr_value_string(JsonValue* val) {
  std::string attr_value = "";
  char buf[12];

  switch(val->get_value_type()) {
    case json_string:
      attr_value = val->get_string_value();
      break;
    case json_true:
      attr_value = "true";
      break;
    case json_false:
      attr_value = "false";
      break;
    case json_integer:
      sprintf(buf, "%10d", val->get_integer_value());
      break;
    case json_float:
      sprintf(buf, "%10d", (int)val->get_float_value());
      break;
    default:
      attr_value = "";
      break;
  }

  return attr_value;
}



int get_attr_value_integer(JsonValue* val) {
  int attr_value = 0;
  std::string str;

  switch(val->get_value_type()) {
    case json_string:
      str = val->get_string_value();
      attr_value = atoi(str.c_str());
      break;
    case json_true:
      attr_value = 1;
      break;
    case json_false:
      attr_value = 0;
      break;
    case json_integer:
      attr_value = val->get_integer_value();
      break;
    case json_float:
      attr_value = (int)val->get_float_value();
      break;
    default:
      break;
  }

  return attr_value;
}




/// compare
int sector_comp(unsigned short s1, unsigned short s2) {
  if(s1 == s2) return 0;

  if(s1 == DATA_SECTOR_LEFT || s2 == DATA_SECTOR_RIGHT) return -1;
  else if(s1 == DATA_SECTOR_RIGHT || s2 == DATA_SECTOR_LEFT) return 1;

  return (s1 < s2 ? -1 : 1);
}


int document_addr_comp(DocumentAddr a1, DocumentAddr a2) {
  int cmp = sector_comp(a1.sector, a2.sector);
  if(cmp != 0) return cmp;

  if(a1.offset == a2.offset) return 0;
  else if(a1.offset == NULL_DOCUMENT) return -1;
  else if(a2.offset == NULL_DOCUMENT) return 1;

  return (a1.offset < a2.offset ? -1 : 1);
}


int phrase_addr_comp(PhraseAddr a1, PhraseAddr a2) {
  int cmp = sector_comp(a1.sector, a2.sector);
  if(cmp != 0) return cmp;

  if(a1.offset == a2.offset) return 0;
  else if(a1.offset == NULL_PHRASE) return -1;
  else if(a2.offset == NULL_PHRASE) return 1;

  return (a1.offset < a2.offset ? -1 : 1);
}

int phrase_data_comp(PhraseData a, PhraseData b) {
  if(!a.value && !b.value) return 0;
  if(!a.value)  return -1;
  if(!b.value)  return 1;

  int cmp = (unsigned char)a.value[0] - (unsigned char)b.value[0];
  if(cmp != 0) return cmp;

  if(IS_ATTR_TYPE_STRING(a.value[0])) {
    return strcmp(a.value+1, b.value+1);
  }
  else {
    return *(int*)(a.value+1) - *(int*)(b.value+1);
  }

  return 0;
}


int reverse_index_comp(InsertReverseIndex i1, InsertReverseIndex i2) {
  int cmp = 0;

  // priority1. document sector asc
  if(i1.doc.addr.offset == NULL_DOCUMENT && i2.doc.addr.offset == NULL_DOCUMENT) return 0;
  if(i1.doc.addr.offset == NULL_DOCUMENT)  return -1;
  if(i2.doc.addr.offset == NULL_DOCUMENT)  return 1;

  cmp = sector_comp(i1.doc.addr.sector, i2.doc.addr.sector);
  if(cmp != 0) return cmp;


  // priority2. phrase value
  if(!i1.phrase.data.value && !i2.phrase.data.value) return 0;
  if(!i1.phrase.data.value)  return -1;
  if(!i2.phrase.data.value)  return 1;
  if(i1.phrase.addr.offset == NULL_PHRASE && i2.phrase.addr.offset == NULL_PHRASE) return 0;
  if(i1.phrase.addr.offset == NULL_PHRASE)  return -1;
  if(i2.phrase.addr.offset == NULL_PHRASE)  return 1;
 
  cmp = (unsigned char)i1.phrase.data.value[0] - (unsigned char)i2.phrase.data.value[0];
  if(cmp != 0) return cmp;

  if(IS_ATTR_TYPE_STRING((unsigned char)i1.phrase.data.value[0])) {
    cmp = strcmp(i1.phrase.data.value+1, i2.phrase.data.value+1);
  } else {
    cmp = *(int*)(i1.phrase.data.value+1) - *(int*)(i2.phrase.data.value+1);
  }
  if(cmp != 0) return cmp;



  // priority3. document sortkey desc
  for(unsigned int i=0; i<SORT_KEY_COUNT; i++) {
    if(i1.doc.data.sortkey[i] != i2.doc.data.sortkey[i]) {
      return i1.doc.data.sortkey[i]>i2.doc.data.sortkey[i] ? 1 : -1;
    }
  }

  // priority4. document id desc
  cmp = i2.doc.data.id - i1.doc.data.id;
  if(cmp != 0) return cmp;

  // priority5. pos asc
  cmp = i1.phrase.pos - i2.phrase.pos;
  if(cmp != 0) return cmp;

  // priority6. delete_flag desc
  return (i2.delete_flag ? 1 : 0) - (i1.delete_flag ? 1 : 0);
}


int search_hit_data_comp(SearchHitData a, SearchHitData b) {
  if(a.empty && b.empty) return 0;
  if(a.empty)  return 1;
  if(b.empty)  return -1;

  for(unsigned int i=0; i<SORT_KEY_COUNT; i++) {
    if(a.sortkey[i] != b.sortkey[i]) {
      return a.sortkey[i]>b.sortkey[i] ? 1 : -1;
    }
  }

  int cmp = b.id-a.id;
  if(cmp != 0) return cmp;

  return a.pos-b.pos;
}


int search_hit_data_comp_weak(SearchHitData a, SearchHitData b) {
  if(a.empty && b.empty) return 0;
  if(a.empty)  return 1;
  if(b.empty)  return -1;

  for(unsigned int i=0; i<SORT_KEY_COUNT; i++) {
    if(a.sortkey[i] != b.sortkey[i]) {
      return a.sortkey[i]>b.sortkey[i] ? 1 : -1;
    }
  }

  return b.id-a.id;
}
