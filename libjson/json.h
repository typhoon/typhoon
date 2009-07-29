/*
 *   JSON C++ Library Ver1.1
 *   Drecom. co. Ltd. 2005.
 *   IMAMURA Shoichi <imamura@drecom.co.jp>
 *
 *   Json Classes handle conversion between C++ string and JsonObject.
 *   Using JSON format, we can easily exchange various data 
 *   with other system programmed by other languages, such as Perl, PHP, JAVA, etc.
 *
 *   Ver1.0->1.1   change constructor interface
 *
 */

#ifndef _JSON_H_
#define _JSON_H_

#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <utility>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Constants
#define JSON_ERR_NOERR 0
#define JSON_ERR_DEFAULT 1
#define JSON_ERR_FORMAT 2
#define JSON_ERR_KEYDOUBLED 3
#define JSON_ERR_INVALIDVALUE 4
#define JSON_ERR_TYPE 5

#define JSON_ERR_OTHER 100

// Types
enum JsonValueType {
    json_string, json_array, json_object, json_integer, json_float, json_true, json_false, json_null
};

typedef class _JsonValue JsonValue;
typedef class _JsonImport JsonImport;
typedef class _JsonExport JsonExport;
typedef class _JsonException JsonException;
typedef std::map<std::string, JsonValue*> JsonObject;
typedef std::vector<JsonValue*> JsonArray;
typedef unsigned int JsonError;

// Sub files
#include "json_value.h"
#include "json_import.h"
#include "json_export.h"
#include "json_exception.h"

#endif
