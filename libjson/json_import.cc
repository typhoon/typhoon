/*
 *  JSON C++ library
 *    Drecom. co. Ltd  2005.
 *    IMAMURA Shoichi (imamura@drecom.co.jp)
 */

#include "config.h"
#include "json.h"


/*
 *   Json import class
 */
void _JsonImport :: _trim (const char* str, unsigned int* import_pos) {
  while(str[*import_pos] == ' ' || str[*import_pos] == '\r' || str[*import_pos] == '\n') {(*import_pos)++;}
}


JsonValue* _JsonImport :: json_import (const std::string& str) {
  return _import(str.c_str(), str.length());
}

JsonValue* _JsonImport :: json_import (const char* str) {
  return _import(str, strlen(str));
}


JsonValue* _JsonImport :: _import(const char* str, unsigned int length) {
  unsigned int import_pos = 0;
  JsonValue* jv = NULL;

  try {
    jv = _import_value(str, &import_pos);

    if(!jv || length != import_pos ) {
      throw JsonException(JSON_ERR_FORMAT);
    }
  } catch (JsonException je) {
    if(jv) {delete jv;}
    return NULL;
  } 

  return jv;
}


JsonValue* _JsonImport :: _import_value (const char* str, unsigned int* import_pos) {
  JsonValue* val = NULL;

  _trim(str, import_pos);
 switch (str[*import_pos]) {
   case 'n' :
     val = _import_null(str, import_pos);
     break;
   case 't' :
     val = _import_true(str, import_pos);
     break;
   case 'f' :
     val = _import_false(str, import_pos);
     break;
   case '0' :
     case '1' : 
     case '2' : 
     case '3' : 
     case '4' : 
     case '5' : 
     case '6' : 
     case '7' : 
     case '8' : 
     case '9' : 
     case '-' : 
       val = _import_number(str, import_pos);
       break;
     case '"' :
       val = _import_string(str, import_pos);
       break;
     case '{' :
       val = _import_object(str, import_pos);
       break;
     case '[' :
       val = _import_array(str, import_pos);
       break;
     default : 
       throw JsonException(JSON_ERR_FORMAT);
  }
  _trim(str, import_pos);

  return val;
}

JsonValue* _JsonImport :: _import_string (const char* str, unsigned int* import_pos) {
  JsonValue* val = NULL;
  char* buf = new char [strlen(str)-(*import_pos)+1];   
  unsigned int buf_pos = 0;  
 
  try {
    if(str[*import_pos] == '"') {
      (*import_pos)++;
    } else {throw JsonException(JSON_ERR_FORMAT);}

    while(str[*import_pos] != '"') {
      if(str[*import_pos] == '\0') {throw JsonException(JSON_ERR_FORMAT);}

      if(str[*import_pos] != '\\') {
        buf[buf_pos++] = str[(*import_pos)++];
      } else {
        (*import_pos)++;
        if(str[*import_pos] == '"') {buf[buf_pos++] = str[(*import_pos)++];}
        else if(str[*import_pos] == '/') {buf[buf_pos++] = str[(*import_pos)++];}
        else if(str[*import_pos] == '\\') {buf[buf_pos++] = str[(*import_pos)++];}
        else if(str[*import_pos] == 'n') {buf[buf_pos++] = '\n';(*import_pos)++;}
        else if(str[*import_pos] == 't') {buf[buf_pos++] = '\t';(*import_pos)++;}
        else if(str[*import_pos] == 'r') {buf[buf_pos++] = '\r';(*import_pos)++;}
        else if(str[*import_pos] == 'b') {buf[buf_pos++] = '\b';(*import_pos)++;}
        else if(str[*import_pos] == 'f') {buf[buf_pos++] = '\f';(*import_pos)++;}
        else if(str[*import_pos] == 'u') {
          (*import_pos)++;
          unsigned char unicode[4];
          for(int i=0;i<4;i++) {
            unsigned char c = str[(*import_pos)];
            if ( c >='0'  &&  c <= '9' ) unicode[i] = c - '0';
            else if ( c >= 'a'  &&  c <= 'f' ) unicode[i] = c - 'a' + 10;
            else if ( c >= 'A'  &&  c <= 'F' ) unicode[i] = c - 'A' + 10;
            else throw JsonException(JSON_ERR_FORMAT);
            (*import_pos)++;
          }

          if(unicode[0] == 0 && unicode[1] == 0) {
            buf[buf_pos] = (unicode[2] << 4) | (unicode[3]);
            buf_pos++;
          } else {
            buf[buf_pos+0] = (0xE0 | unicode[0]);
            buf[buf_pos+1] = (unicode[1] << 4) | unicode[2];
            buf[buf_pos+1] = 0x80 | (0x3F & (buf[buf_pos+1] >> 2));
            buf[buf_pos+2] = (unicode[2] << 4) | unicode[3];
            buf[buf_pos+2] = 0x80 |  (0x3F & buf[buf_pos+2]);

            buf_pos += 3;
          }
        }    
        else {throw JsonException(JSON_ERR_FORMAT);}
      }
    }

    (*import_pos)++;
    _trim(str, import_pos);
        
    buf[buf_pos] = '\0';
    val = new JsonValue(buf);
    delete[] buf;
  } catch (JsonException je) {
    if(buf) {delete[] buf;}
    throw JsonException (je.get_errcode());
  } 
    
  return val; 
}


JsonValue* _JsonImport :: _import_number (const char* str, unsigned int* import_pos) {
  char* buf = new char [strlen(str)-(*import_pos)+1];
  unsigned int buf_pos = 0;
  JsonValueType type = json_integer;
  JsonValue* val = NULL;

  try {
    if(str[*import_pos] == '-') {
      buf[buf_pos++] = str[(*import_pos)++];
    }

    if(!isdigit(str[*import_pos])) {throw JsonException(JSON_ERR_FORMAT);}
    if(str[*import_pos] != '0') {
      while(isdigit(str[*import_pos])) {
        buf[buf_pos++] = str[(*import_pos)++];
      } 
    } else {
      buf[buf_pos++] = str[(*import_pos)++];
    }

    if(str[*import_pos] == '.') {
      type = json_float; 
      buf[buf_pos++] = str[(*import_pos)++];

      if(!isdigit(str[*import_pos])) {throw JsonException(JSON_ERR_FORMAT);}
      while(isdigit(str[*import_pos])) {
        buf[buf_pos++] = str[(*import_pos)++];
      }
    }

    if(str[*import_pos] == 'E' || str[*import_pos] == 'e') {
      type = json_float;
      buf[buf_pos++] = str[(*import_pos)++];
      if(str[*import_pos] == '+' || str[*import_pos] == '-') {
        buf[buf_pos++] = str[(*import_pos)++];
      }
      if(!isdigit(str[*import_pos])) {throw JsonException(JSON_ERR_FORMAT);}
      while(isdigit(str[*import_pos])) {
        buf[buf_pos++] = str[(*import_pos)++];
      } 
    }
    buf[buf_pos] = '\0';

    if(type == json_integer) {
      int intval = atoi(buf);
      val = new JsonValue (intval);
    } else {
      double floatval = atof(buf);
      val = new JsonValue (floatval);
    } 
    delete[] buf;

    _trim(str, import_pos);
  } catch (JsonException je) {
    delete[] buf;
    throw JsonException (je.get_errcode());
  }

  return val;

}

JsonValue* _JsonImport :: _import_object (const char* str, unsigned int* import_pos) {
  JsonValue* object = new JsonValue (json_object);
  JsonValue* tag = NULL;
  JsonValue* val = NULL;

  try {
    if(str[*import_pos] == '{') {
      (*import_pos)++;
    } else {throw JsonException(JSON_ERR_FORMAT);}


    bool init_flag = true;   
    _trim(str, import_pos);
    while(str[*import_pos] != '}') {
      if(init_flag) {init_flag= false;}
      else if(str[*import_pos] == ',') {
        (*import_pos)++;
      } else {throw JsonException(JSON_ERR_FORMAT);}
       
      _trim(str, import_pos);
      tag = _import_string(str, import_pos);
      if(str[*import_pos] != ':') {throw JsonException(JSON_ERR_FORMAT);}
      (*import_pos)++;
      val = _import_value(str, import_pos);

      object->add_to_object(std::string((char*)(tag->get_value())), val);
      delete tag;
      tag=NULL,val=NULL;
    }
    _trim(str, import_pos);

    (*import_pos)++;
  } catch (JsonException je) {
   if(tag) delete tag;
   if(val) delete val;
   delete object; 
   throw JsonException(je.get_errcode());
  }

  return object;
}

JsonValue* _JsonImport :: _import_array  (const char* str, unsigned int* import_pos) {
  JsonValue* array = new JsonValue (json_array);

  try {
    if(str[*import_pos] == '[') {
      (*import_pos)++;
    } else {throw JsonException(JSON_ERR_FORMAT);}

    bool init_flag = true;   
    _trim(str, import_pos);
    while(str[*import_pos] != ']') {
      if(init_flag) {init_flag = false;}
      else if(str[*import_pos] == ',') {
        (*import_pos)++;
      } else {throw JsonException(JSON_ERR_FORMAT);}
       
      JsonValue* val = _import_value(str, import_pos);
      array->add_to_array(val);
    }
    _trim(str, import_pos);

    (*import_pos)++;
  } catch (JsonException je) {
    delete array;
    throw JsonException(je.get_errcode());
  } 

  return array;
}

JsonValue* _JsonImport :: _import_true (const char* base_str, unsigned int* import_pos) {
  const char* str = base_str+(*import_pos);
  if(strlen(str) > 3 && strncmp(str,"true",4) == 0) {*import_pos += 4;}
  else {throw JsonException(JSON_ERR_FORMAT);}
  _trim(base_str, import_pos);
  
  return new JsonValue(json_true) ;
}

JsonValue* _JsonImport :: _import_false (const char* base_str, unsigned int* import_pos) {
  const char* str = base_str+(*import_pos);
  if(strlen(str) > 4 && strncmp(str,"false",5) == 0) {*import_pos += 5;}
  else {throw JsonException(JSON_ERR_FORMAT);}
  _trim(base_str, import_pos);
  
  return new JsonValue(json_false) ;
}

JsonValue* _JsonImport :: _import_null (const char* base_str, unsigned int* import_pos) {
  const char* str = base_str+(*import_pos);
  if(strlen(str) > 3 && strncmp(str,"null",4) == 0) {*import_pos += 4;}
  else {throw JsonException(JSON_ERR_FORMAT);}
  _trim(base_str, import_pos);

  return new JsonValue() ;
}


