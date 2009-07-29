/*
 *   JSON C++ Library
 *   Drecom. co. Ltd. 2005.
 *   IMAMURA Shoichi <imamura@drecom.co.jp>
 *
 *   JsonHanlder class handles conversion between C++ string and JsonObject.
 *   Using JSON format, we can easily exchange various data 
 *   with other system programmed by other language like Perl, PHP, JAVA.
 */

#ifndef _JSON_IMPORT_H_
#define _JSON_IMPORT_H_

#include "json.h"

class _JsonImport {
public:
  ~_JsonImport() {}
 
  static JsonValue*  json_import(const std::string&);
  static JsonValue*  json_import(const char*);
private:
  _JsonImport() {}

  static void _trim(const char*, unsigned int*);

  static JsonValue*  _import(const char*, unsigned int);
  static JsonValue*  _import_value(const char*, unsigned int*);
  static JsonValue*  _import_string(const char*, unsigned int*);
  static JsonValue*  _import_array(const char*, unsigned int*);
  static JsonValue*  _import_object(const char*, unsigned int*);
  static JsonValue*  _import_true(const char*, unsigned int*);
  static JsonValue*  _import_false(const char*, unsigned int*);
  static JsonValue*  _import_null(const char*, unsigned int*);
  static JsonValue*  _import_number(const char*, unsigned int*);
};


#endif
