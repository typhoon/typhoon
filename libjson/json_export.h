/*
 *   JSON C++ Library
 *   Drecom. co. Ltd. 2005.
 *   IMAMURA Shoichi <imamura@drecom.co.jp>
 *
 *   JsonHanlder class handles conversion between C++ string and JsonObject.
 *   Using JSON format, we can easily exchange various data 
 *   with other system programmed by other language like Perl, PHP, JAVA.
 */

#ifndef _JSON_EXPORT_H_
#define _JSON_EXPORT_H_

#include "json.h"

class _JsonExport {
public:
  _JsonExport() {}
  ~_JsonExport() {}   

  static std::string json_export(JsonValue* const); 

private:
  static std::string _export_value(JsonValue* const);
  static std::string _export_string(JsonValue* const);
  static std::string _export_array(JsonValue* const);
  static std::string _export_object(JsonValue* const);
  static std::string _export_true(JsonValue* const);
  static std::string _export_false(JsonValue* const);
  static std::string _export_null(JsonValue* const);
  static std::string _export_number(JsonValue* const);
};

#endif
