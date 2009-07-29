/*
 *   JSON C++ Library
 *   Drecom. co. Ltd. 2005.
 *   IMAMURA Shoichi <imamura@drecom.co.jp>
 *
 *   JsonHanlder class handles conversion between C++ string and JsonObject.
 *   Using JSON format, we can easily exchange various data 
 *   with other system programmed by other language like Perl, PHP, JAVA.
 */

#ifndef _JSON_VALUE_H_
#define _JSON_VALUE_H_

#include "json.h"

class _JsonValue {
public:
    _JsonValue();
    _JsonValue(int);
    _JsonValue(double);
    _JsonValue(const char*);
    _JsonValue(std::string);
    _JsonValue(JsonValueType);
    ~_JsonValue();
    
    // data access
    JsonValueType   get_value_type() {return _type;}
    void*     get_value() {return _value;}  //This is not safety
    int      get_integer_value();
    double   get_float_value();
    const char*     get_string_value();
    JsonObject* get_object_value();
    JsonArray*  get_array_value();
    std::vector<std::string> get_tags();

    bool            is_true();
    bool            is_false();
    bool            is_null();

    JsonValue*     get_value_by_tag(std::string);
    JsonValue*     get_value_by_index(unsigned int);


    // data construct
    bool           add_to_object(std::string, JsonValue*);
    bool           add_to_array(JsonValue*);
    void           set_data(JsonValueType, void*);

    void         clear(void);
private:
    JsonValueType _type;
    void* _value;
    unsigned int _refer_cnt;

};


#endif
