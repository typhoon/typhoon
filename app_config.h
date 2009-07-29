#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__


#include "common.h"
#include "file_access.h"


class AppConfig {
public:
  std::string   path;
  ATTR_TYPE_MAP attrs;
  unsigned int page_size;
  unsigned int block_size;
  unsigned int phrase_length;

  // command line options
  std::string log_file;
  std::string pid_file;
  std::string data_file;

  bool debug;

  bool daemon;
  unsigned short port;
  unsigned int max_document_length;

  unsigned int max_offset;
  unsigned int max_limit;
  unsigned int max_words;

  bool load_conf();
  bool save_conf();
  bool directory_check();
  bool import_attrs(const char*);

  AppConfig();
  ~AppConfig() {}

  bool test();
  void dump();

private:
  bool set_attributes(JsonValue*);
  bool set_sortkeys(JsonValue*);
};


#endif
