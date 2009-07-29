/*
 *  JSON C++ library
 *    Drecom. co. Ltd  2005.
 *    IMAMURA Shoichi (imamura@drecom.co.jp)
 */

#include "config.h"
#include "json.h"

/*
 *  JSON export class
 */
std::string _JsonExport :: json_export(JsonValue* const val) {
  if(!val) return "";
  return _export_value(val);
}

std::string _JsonExport :: _export_value(JsonValue* const val) {
  switch (val->get_value_type()) {
    case json_null:
        return _export_null(val);
    case json_true:
        return _export_true(val);
    case json_false : 
        return _export_false(val);
    case json_string :
        return _export_string(val);
    case json_integer :
    case json_float : 
        return _export_number(val);
    case json_array :
        return _export_array(val);
    case json_object : 
        return _export_object(val);
    default : 
        throw JsonException (JSON_ERR_INVALIDVALUE);
  }
}

std::string _JsonExport :: _export_null (JsonValue* const val) {
  return "null";
}


std::string _JsonExport :: _export_true (JsonValue* const val) {
  return "true";
}

std::string _JsonExport :: _export_false (JsonValue* const val) {
  return "false";
}

std::string _JsonExport :: _export_string(JsonValue* const val) {
  std::string res = "";
  std::string begin = "\"";
  std::string end = "\"";

  const char* str = val->get_string_value();
  unsigned int offset = 0;
  while(str[offset] != '\0') {
    if(str[offset] == '\t') {
        res.append(1,'\\').append(1,'t');
    } else if(str[offset] == '/') {
        res.append(1,'\\').append(1,'/');
    } else if(str[offset] == '\\') {
        res.append(1,'\\').append(1,'\\');
    } else if(str[offset] == '\n') {
        res.append(1,'\\').append(1,'n');
    } else if(str[offset] == '\r') {
        res.append(1,'\\').append(1,'r');
    } else if(str[offset] == '\f') {
        res.append(1,'\\').append(1,'f');
    } else if(str[offset] == '\b') {
        res.append(1,'\\').append(1,'f');
    } else {
        res.append(1,str[offset]); 
    }

    offset++;
  }

  return begin + res + end;
}

std::string _JsonExport :: _export_number(JsonValue* const val) {
  char* num_str = new char[1024];
  std::string ret = "";

  if(val->get_value_type() == json_integer) {
    sprintf( num_str, "%d", *((int*)(val->get_value())) );
  } else {
    sprintf( num_str, "%f", *((double*)(val->get_value())) );
  }

  ret = ret + num_str;
  delete[] num_str;

  return ret;
}


std::string _JsonExport :: _export_array (JsonValue* const val) {
  std::string ret_start = "[";
  std::string ret_end = "]";
  std::string ret = "";

  JsonArray* array = (JsonArray*)(val->get_value());
  for(JsonArray::iterator i=array->begin();i!=array->end();i++) {
      if(i!=array->begin()) {ret = ret + ",";}
      ret = ret + _export_value(*i);
  } 

  return ret_start + ret + ret_end; 
}

std::string _JsonExport :: _export_object (JsonValue* const val) {
  std::string ret_start = "{";
  std::string ret_end = "}";
  std::string ret = "";

  JsonObject* object = (JsonObject*)(val->get_value());
  for(JsonObject::iterator i=object->begin();i!=object->end();i++) {
    if(i!=object->begin()) {ret=ret+",";}

    ret = ret + "\"" + (*i).first + "\"" + ":" + _export_value((*i).second); 
  }

  return ret_start + ret + ret_end;
}


