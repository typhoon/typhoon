/*
 *   JSON C++ Library
 *   Drecom. co. Ltd. 2005.
 *   IMAMURA Shoichi <imamura@drecom.co.jp>
 *
 *   JsonHanlder class handles conversion between C++ string and JsonObject.
 *   Using JSON format, we can easily exchange various data 
 *   with other system programmed by other language like Perl, PHP, JAVA.
 */
#ifndef _JSON_EXCEPTION_H_
#define _JSON_EXCEPTION_H_

#include "json.h"

class _JsonException {
public :
    _JsonException() {_errcode = JSON_ERR_DEFAULT;}
    _JsonException(JsonError errcode) {_errcode = errcode;}
    ~_JsonException() {}
    void message();
    JsonError get_errcode() {return _errcode;}

private :
    JsonError _errcode;
};


#endif
