/*
 *  JSON C++ library
 *    Drecom. co. Ltd  2005.
 *    IMAMURA Shoichi (imamura@drecom.co.jp)
 */

#include "config.h"
#include "json.h"


/*  JSON Exception handle class */
void _JsonException :: message () {
    switch (_errcode) {
        case JSON_ERR_DEFAULT :
            std::cout << "error has occured\n";
        case JSON_ERR_NOERR : 
            std::cout << "no error has occured\n";
            break;
        case JSON_ERR_FORMAT : 
            std::cout << "import format is not correct\n";
            break;
        case JSON_ERR_KEYDOUBLED : 
            std::cout << "object key is doubled\n";
            break;
        case JSON_ERR_INVALIDVALUE :
            std::cout << "invalid value type exists\n";
            break;
        default : 
            std::cout << "unknown error occured\n";
            break;
    }
}

