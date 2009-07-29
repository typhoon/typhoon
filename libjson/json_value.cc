/*
 *  JSON C++ library
 *    Drecom. co. Ltd  2005.
 *    IMAMURA Shoichi (imamura@drecom.co.jp)
 */

#include "config.h"
#include "json.h"


/*
 * JsonValue object constructor
 */
_JsonValue :: _JsonValue (void) 
: _type(json_null), _value(NULL), _refer_cnt(1) {    
}     

_JsonValue :: _JsonValue (int intval) 
: _refer_cnt(1) {
  _type = json_integer;
  _value = new int (intval);
}

_JsonValue :: _JsonValue (double floatval) 
: _refer_cnt(1) {
  _type = json_float;
  _value = new double (floatval);
}

_JsonValue :: _JsonValue (const char* str) 
: _refer_cnt(1) {
  _type = json_string;
  _value = new char[strlen(str)+1];

  strcpy((char*)_value, str);
}

_JsonValue :: _JsonValue(std::string s) {
  _type = json_string;
  _value = new char[s.length()+1];

  strcpy((char*)_value, s.c_str());
}


_JsonValue :: _JsonValue(JsonValueType type)
: _type(type), _refer_cnt(1) {
  switch(type) {
    case json_integer:
        _value = new int(0);
        break;
    case json_float:
        _value = new double (0.0);
        break;
    case json_array:
        _value = new JsonArray();
        break;
    case json_object:
        _value = new JsonObject();
        break; 
    default:
        _value = NULL;
        break;
  }
}



_JsonValue :: ~_JsonValue () {
   clear();
}


// data access
int _JsonValue :: get_integer_value() {
  if(_type != json_integer) throw JsonException(JSON_ERR_TYPE);

  return *((int*)_value);
}

double _JsonValue :: get_float_value() {
  if(_type != json_float) throw JsonException(JSON_ERR_TYPE);

  return *((double*)_value);
}

const char* _JsonValue :: get_string_value() {
  if(_type != json_string) throw JsonException(JSON_ERR_TYPE);

  return (const char*)_value;
}

JsonObject* _JsonValue :: get_object_value() {
  if(_type != json_object) throw JsonException(JSON_ERR_TYPE);
 
  return (JsonObject*)_value;
}

JsonArray* _JsonValue :: get_array_value() {
  if(_type != json_array) throw JsonException(JSON_ERR_TYPE);

  return (JsonArray*)_value;
}


bool _JsonValue :: is_true() {
  return _type == json_true;
}

bool _JsonValue :: is_false() {
  return _type == json_false;
}

bool _JsonValue :: is_null() {
  return _type == json_null;
}


JsonValue* _JsonValue :: get_value_by_index (unsigned int idx) {
  if(_type != json_array)  throw JsonException(JSON_ERR_TYPE);

  JsonArray* a = (JsonArray*)_value;
  if( a->size() <= idx ) {return NULL;}
  return a->at(idx);
}


JsonValue* _JsonValue :: get_value_by_tag (std::string key)  {
  if(_type != json_object) throw JsonException(JSON_ERR_TYPE);

  JsonObject* o = (JsonObject*)_value;
  JsonObject::iterator value_itr = o->find(key);

  if(value_itr == o->end()) {return NULL;}
  else {
    return value_itr->second;
  }
}

std::vector<std::string> _JsonValue :: get_tags() {
  if(_type != json_object) throw JsonException(JSON_ERR_TYPE);

  JsonObject* o = (JsonObject*)_value;
  std::vector<std::string> tags;
  for(JsonObject::iterator value_itr = o->begin(); value_itr != o->end(); value_itr++) {
    tags.push_back(value_itr->first);
  }

  return tags;
}


// data construction
bool _JsonValue :: add_to_object (std::string key, JsonValue* value) {
  std::pair<JsonObject::iterator, bool> insert_res;

  if(get_value_type() != json_object) return false;

  insert_res = ((JsonObject*)_value)->insert(std::make_pair(key, value));
  if(!insert_res.second)  return false;

  return true;
}

bool _JsonValue :: add_to_array (JsonValue* element) {
  if(get_value_type() != json_array) return false;

  ((JsonArray*)_value)->push_back(element);
  return true;
}


void _JsonValue :: set_data(JsonValueType t, void* v) {
  if(_value) clear();
  _type = t;
  _value = v;
}


void _JsonValue :: clear() {
  if(_type == json_string) {
    delete [] (char*)_value;
  } else if(_type == json_float) {
    delete (double*)_value;
  } else if(_type == json_integer) { 
    delete (int*)_value;
  } else if(_type == json_object) {
    JsonObject* o = (JsonObject*)_value;
    for(JsonObject::iterator i=o->begin();i != o->end(); i++) {
      JsonValue* v = i->second;
        delete v;
        i->second = NULL;
    }
    o->clear();
    delete o;
    _value = NULL; 
  } else if(_type == json_array) {
    JsonArray* a = (JsonArray*)_value;
    for(JsonArray::iterator i=a->begin();i != a->end(); i++) {
      delete (*i);
    } 
    a->clear();
    delete a;
    _value = NULL;
  } 
}

