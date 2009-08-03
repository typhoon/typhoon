/*****************************************************************************
 *  app_config.h
 *    application configulation class.
 * 
 *  $Author: imamura $
 *  $Date:: 2009-03-31 18:55:26 +0900#$
 *
 *  Copyright (C) 2008- Drecom Co.,Ltd. All Rights Reserved.
 *****************************************************************************/

#include <time.h>

#include "common.h"
#include "app_config.h"

/***********************************************************
 *    AppConfig base class
 ***********************************************************/
AppConfig::AppConfig() {
  page_size = getpagesize() * 16; 
  max_offset = 10000;
  max_limit  = 10000;
  max_words  = 10;
}


/////////////////////////////////////////////////////
// public
/////////////////////////////////////////////////////
bool AppConfig::load_conf() {
    std::string conf_file = path + "/info.dat";

    if(!FileAccess::is_file(conf_file)) return false;
    FILE* fp = fopen(conf_file.c_str(), "rb");
    if(!fp) {
      return false;
    }

    if(fread(&block_size, sizeof(unsigned int), 1, fp) != 1) {
      fclose(fp);
      return false;
    }

    unsigned int attr_size; 
    if(fread(&attr_size, sizeof(unsigned int), 1, fp) != 1) {
      fclose(fp);
      return true; // old version config
    }

    attrs.clear();
    for(unsigned int i=0; i<attr_size; i++) {
      unsigned int attr_len = 0;
      if(fread(&attr_len, sizeof(unsigned int), 1, fp) != 1) {
        fclose(fp);
        return false;
      }

      char* str = new char[attr_len + 1];
      if(fread(str, sizeof(char), attr_len, fp) != attr_len) {
        fclose(fp);
        return false;
      }
      str[attr_len] = '\0';
      AttrDataType attr_type;
      if(fread(&attr_type, sizeof(AttrDataType), 1, fp) != 1) {
        fclose(fp);
        return false;
      }

      attrs.insert( std::map<std::string, AttrDataType>::value_type(std::string(str), attr_type) ); 
      delete[] str;
    }

    if(fread(&phrase_length, sizeof(unsigned int), 1, fp) != 1) {
      fclose(fp);
      phrase_length = 1;
      return true; // old version config
    }

    fclose(fp);

    return true;
}


bool AppConfig::save_conf() {
    std::string conf_file = path + "/info.dat";

    FILE* fp = fopen(conf_file.c_str(), "wb");
    if(!fp) {
        std::cerr << "Config file open failed";
        return false;
    }

    fwrite(&block_size, sizeof(unsigned int), 1, fp);

    unsigned int attr_size = attrs.size();
    fwrite(&attr_size, sizeof(unsigned int), 1, fp);
    for(ATTR_TYPE_MAP::iterator itr=attrs.begin(); itr!=attrs.end(); itr++) {
        unsigned int len = itr->first.length();
        fwrite(&len, sizeof(unsigned int), 1, fp);
        fwrite((itr->first).c_str(), sizeof(char), len, fp);
        AttrDataType attr_type = itr->second;
        fwrite(&attr_type, sizeof(AttrDataType), 1, fp);
    }
    fwrite(&phrase_length, sizeof(unsigned int), 1, fp); 

    fclose(fp);
    return true;
}


bool AppConfig::directory_check() {
  if(!FileAccess::is_directory(path.c_str())) {
    std::cerr << "Base path not exists" << "\n";
    return false;
  }
  if(!FileAccess::create_directory(path + "/tmp")) {
    std::cerr << "Directory create failed" << "\n";
    return false;
  }

  return true;
}


bool AppConfig::import_attrs_from_string(const char* str) {
  JsonValue* settings_val = JsonImport::json_import(str);
  if(!settings_val) {
    std::cerr << "[ERROR] invalid JSON format.\n";
    return false;
  }
  if(settings_val->get_value_type() != json_object) {
    std::cerr << "[ERROR] settings must be object type.\n";
    return false;
  }

  return import_attrs_from_json(settings_val);
}

bool AppConfig::import_attrs_from_json(JsonValue* datatype_val) {
  attrs.clear();

  JsonValue* column_val = datatype_val->get_value_by_tag("columns");
  JsonValue* sort_val = datatype_val->get_value_by_tag("sortkeys");
  if(!column_val || column_val->get_value_type() != json_object) {
    std::cerr << "[ERROR] invalid column format.\n";
    return false;
  }

  try {
    if(!set_attributes(column_val)) return false;
    if(!set_sortkeys(sort_val)) return false;
  } catch(JsonException e) {
    return false;
  }

  return true;
}


bool AppConfig::set_attributes(JsonValue* column_val) {
  WORD_SET tags = column_val->get_tags();

  bool pkey_exists = false;
  unsigned char attr_id = 1;
  for(unsigned int i=0; i<tags.size(); i++) {
    std::string attr_name = tags[i];

    if(i > MAX_ATTR_TYPE) {
      std::cerr << "[WARNING] Too many attributes, ignore.\n";
      continue;
    }
 
    if(attr_name.length() > MAX_ATTR_NAME_LENGTH) {
      std::cerr << "[WARNING] Too long attribute name, ignore.\n";
      continue;
    }

    JsonValue* val = column_val->get_value_by_tag(attr_name);
    if(!val) {
      std::cerr << "[ERROR] Invalid attribute\n";
      return false;
    }

    AttrDataType attr_type = {ATTR_TYPE_STRING, false, false, true, false, false, 0, 0};
    std::string attr_val = val->get_string_value();
    WORD_SET attr_details = split(attr_val, ",");
    for(unsigned int j=0; j<attr_details.size(); j++) {
      if(attr_details[j] == "pkey") {
        if(pkey_exists) {
          std::cerr << "[ERROR] Primary key can be defined only one column.\n";
          return false;
        }
        attr_type.header = CREATE_ATTR_HEADER(i+1, ATTR_TYPE_INTEGER);
        attr_type.pkey_flag = true;
        attr_type.index_flag = false;
        attr_type.bit_len = 32;
      } else if(attr_details[j] == "int" || attr_details[j] == "int32" || attr_details[j] == "integer") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_INTEGER);
        attr_type.bit_len = 32;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "bool" || attr_details[j] == "boolean") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_INTEGER);
        attr_type.bit_len = 1;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "int8" || attr_details[j] == "tinyint") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_INTEGER);
        attr_type.bit_len = 8;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "int16" || attr_details[j] == "smallint") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_INTEGER);
        attr_type.bit_len = 16;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "fulltext") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_STRING);
        attr_type.fulltext_flag = true;
        attr_type.bit_len = 0;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "string") {
        attr_type.header = CREATE_ATTR_HEADER(attr_id, ATTR_TYPE_STRING);
        attr_type.fulltext_flag = false;
        attr_type.bit_len = 0;
        attr_type.pkey_flag = false;
      } else if(attr_details[j] == "index") {
        attr_type.index_flag = true;
      } else if(attr_details[j] == "noindex") {
        attr_type.index_flag = false;
      } else {
        std::cerr << "[ERROR] Invalid attribute specified\n";
        return false;
      }
    }

    if(attr_type.pkey_flag) { 
      pkey_exists = true;
    }

    attrs.insert( std::map<std::string, AttrDataType>::value_type(attr_name, attr_type) );
    attr_id++;
  }

  if(!pkey_exists) {
    std::cerr << "[ERROR]There must be a single primary key(type: pkey)\n";
    return false;
  }

  return true;
}

bool AppConfig::set_sortkeys(JsonValue* sort_val) {
  if(!sort_val) return true;

  unsigned char next_bit=0;
  for(unsigned int i=0; i<sort_val->get_array_value()->size(); i++) {
    JsonValue* val = sort_val->get_array_value()->at(i);
    if(val->get_value_type() != json_string) {
      std::cout << "[ERROR] sortkey must specify 'column asc/desc'\n";
      return false;
    }
    std::string s = val->get_string_value();
    WORD_SET wset = split(s, ",");
    ATTR_TYPE_MAP::iterator it=attrs.find(wset[0]);
    if(it==attrs.end()) {
      std::cout << "[ERROR] invalid sort key\n";
      return false;
    }
    if(it->second.bit_len == 0) {
      std::cerr << "[ERROR] Sortkey can specify to int/smallint/tinyint/boolean columns. \n";
      return false;
    }
    if(next_bit > MAX_SORT_BIT) {
      std::cerr << "[ERROR] Sortkey must be limited within 128bits. \n";
      return false;
    }
    it->second.sort_flag = true;
    it->second.bit_from = next_bit;

    if(wset.size() > 1) {
      if(wset[1] == "asc") it->second.bit_reverse_flag = false;
      if(wset[1] == "desc") it->second.bit_reverse_flag = true;
    }

    next_bit = next_bit+it->second.bit_len;
  }

  return true;
}



bool AppConfig::test() {
  std::cout << "attribute parse test\n";
  std::string s;
  ATTR_TYPE_MAP::iterator it;
  AttrDataType d;

  s = "{\"columns\":{\"id\": \"pkey\"}}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("id");
  if(it==attrs.end()) return false;
  d = it->second;
  if(d.header != CREATE_ATTR_HEADER(1, ATTR_TYPE_INTEGER) || d.index_flag != false || d.pkey_flag != true) {
    return false;
  } 

  s = "{\"columns\":{\"id\": \"pkey,index\"}}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("id");
  if(it==attrs.end()) return false;
  d = it->second;
  if(d.header != CREATE_ATTR_HEADER(1, ATTR_TYPE_INTEGER) || d.index_flag != true || d.pkey_flag != true) {
    return false;
  }

  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\"}}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("id");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != false || d.pkey_flag != true) {
    return false;
  }
  it = attrs.find("name");
  if(it==attrs.end()) return false;
  d = it->second;
  if(!IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false) {
    return false;
  }


  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\", \"content\":\"fulltext\"}}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("content");
  if(it==attrs.end()) return false;
  d = it->second;
  if(!IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false || d.fulltext_flag != true || d.sort_flag != false) {
    return false;
  }

  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\", \"active_flag\":\"bool,noindex\"}, \"sortkeys\":[\"active_flag\"]}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("active_flag");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != false || d.pkey_flag != false || d.fulltext_flag != false || d.sort_flag != true ||
     d.bit_from != 0 || d.bit_len != 1) {
    return false;
  }

  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\", \"active_flag\":\"bool\", \"gender\":\"bool\", \"age\":\"tinyint\"}, \"sortkeys\":[\"active_flag\", \"gender\", \"age\"]}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("gender");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false || d.fulltext_flag != false || d.sort_flag != true ||
     d.bit_from != 1 || d.bit_len != 1) {
    return false;
  }
  it = attrs.find("age");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false || d.fulltext_flag != false || d.sort_flag != true ||
     d.bit_from != 2 || d.bit_len != 8) {
    return false;
  }


  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\", \"data1\":\"integer\", \"data2\":\"integer\", \"data3\":\"integer\", \"data4\":\"smallint\", \"data5\":\"smallint\"}, \"sortkeys\":[\"data1\", \"data2\", \"data3\", \"data4\", \"data5,desc\"]}";
  std::cout << s << "\n";
  if(!import_attrs_from_string(s.c_str())) return false;
  it = attrs.find("data4");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false || d.fulltext_flag != false || d.sort_flag != true ||
     d.bit_from != 96 || d.bit_len != 16) {
    return false;
  }
  it = attrs.find("data5");
  if(it==attrs.end()) return false;
  d = it->second;
  if(IS_ATTR_TYPE_STRING(d.header) || d.index_flag != true || d.pkey_flag != false || d.fulltext_flag != false || d.sort_flag != true ||
     d.bit_from != 112 || d.bit_len != 16) {
    return false;
  }
  
  s = "{\"columns\":{\"id\":\"pkey\", \"name\":\"string\", \"data1\":\"integer\", \"data2\":\"integer\", \"data3\":\"integer\", \"data4\":\"integer\", \"extend\":\"boolean\"}, \"sortkeys\":[\"data1\", \"data2\", \"data3\", \"data4\", \"extend\"]}";
  std::cout << s << "\n";
  if(import_attrs_from_string(s.c_str())) return false; // too many sort value


  std::cout << "error pattern test\n";
  s = "{dsafsasffas"; // incomplete JSON
  if(import_attrs_from_string(s.c_str())) return false;
  s = "{\"name\":\"string\"}"; // not exists pkey
  if(import_attrs_from_string(s.c_str())) return false;

 
  return true;
}


void AppConfig::dump() {
  for(ATTR_TYPE_MAP::iterator it=attrs.begin(); it!= attrs.end(); it++) {
    std::string attr_name = it->first;
    AttrDataType attr_type = it->second;
    std::cout << (unsigned int)(attr_type.header & 0x7F) << "\t" << attr_name << "\t" << 
                 (IS_ATTR_TYPE_STRING(attr_type.header) ? "string" : "integer") << "\t" <<
                 (attr_type.pkey_flag ? "pkey" : "-") << "\t" << (attr_type.fulltext_flag ? "fulltext" : "-") << "\t" <<
                 (attr_type.index_flag ? "index" : "-") << "\t" << (attr_type.sort_flag ? "sortkey" : "-") << "\t" << 
                 (attr_type.bit_reverse_flag ? "asc" : "desc") << "\t" << (unsigned int)(attr_type.bit_from) << "\n";
  }
}
