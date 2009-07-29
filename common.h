/****************************************************************
 *  common.h
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-07-29 18:54:26 +0900#$
 *
 ****************************************************************/

#ifndef __COMMON_H__
#define __COMMON_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

// Standard & System libraries
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <math.h>


// STL libraries
#include <vector>
#include <string>
#include <list>
#include <iterator>
#include <map>
#include <iostream>
#include <fstream>
#include <algorithm>

// Application libraries
#include <json.h>
#include "charset.h"



// common definitions
#define MAX_GENERATION              100000000 
#define MAX_PHRASE_LENGTH           1000
#define MAX_PATH_LENGTH             255
#define MAX_DOC_PHRASE              10000
#define MAX_ATTR_NAME_LENGTH        255
#define MAX_ATTR_TYPE               100
#define MAX_SECTOR                  0x7FFF
#define MAX_REVERSE_INDEX_BLOCK     20
#define MAX_REGULAR_INDEX_BLOCK     100
#define MAX_PHRASE_POS              0x7F
#define MAX_SORT_BIT                0x7F
#define MAX_DOCUMENT_CACHE          100

#define MIN_MEMORY_BLOCK            10
#define MAX_MEMORY_BLOCK            65535  // 64K * 64K = 4G

#define DATA_SECTOR_LEFT  0x8000
#define DATA_SECTOR_RIGHT 0x8001


// #define MINIMUM_MEMORY_BLOCK 128
#define MINIMUM_MEMORY_BLOCK 32
#define SMALL_MEMORY_BLOCK   512
#define MEDIUM_MEMORY_BLOCK  1024
#define LARGE_MEMORY_BLOCK   4096
#define HUGE_MEMORY_BLOCK    8192


#define DATA_TYPE_DEFAULT            0x00000000
#define DATA_TYPE_PHRASE_DATA        0x01000000
#define DATA_TYPE_PHRASE_ADDR        0x02000000
#define DATA_TYPE_DOCUMENT_DATA      0x03000000
#define DATA_TYPE_DOCUMENT_ADDR      0x04000000
#define DATA_TYPE_REGULAR_INDEX      0x05000000
#define DATA_TYPE_REVERSE_INDEX      0x06000000
#define DATA_TYPE_DOCUMENT_INFO      0x07000000
#define DATA_TYPE_PHRASE_INFO        0x08000000
#define DATA_TYPE_REGULAR_INDEX_INFO 0x09000000
#define DATA_TYPE_REVERSE_INDEX_INFO 0x0A000000


#define VAL_TO_FILE_TYPE(val)     ( ((val) & 0x0F000000) )
#define VAL_TO_FILE_PAGE(val)     ( ((val) & 0x00FFFFFF) )


#define INDEX_WAIT_DURATION 100000
#define SBM_WAIT_DURATION 500
#define FILE_WAIT_DURATION 10000


#define LOG_LEVEL_DEBUG   0
#define LOG_LEVEL_INFO    1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR   3
#define LOG_LEVEL_FATAL   4


#define IS_INDEX_HEADER(idx) ( (0x80000000 & (idx)) == 0x80000000 )
#define IS_INDEX_SECTOR(idx) ( (0x40000000 & (idx)) == 0x40000000 )
#define IS_INDEX_HEADER_FIRST(idx)  ( IS_INDEX_HEADER(idx) && !IS_INDEX_SECTOR(idx) )
#define IS_INDEX_HEADER_SECOND(idx) ( IS_INDEX_HEADER(idx) && IS_INDEX_SECTOR(idx) )
#define IS_INDEX_BODY(idx)          ( !IS_INDEX_HEADER(idx) )
#define IS_INDEX_BODY_FIRST(idx)    ( !IS_INDEX_HEADER(idx) && !IS_INDEX_SECTOR(idx) )
#define IS_INDEX_BODY_SECOND(idx)   ( !IS_INDEX_HEADER(idx) && IS_INDEX_SECTOR(idx) )


#define REG_INDEX_DOCUMENT_OFFSET(idx) ( 0x00FFFFFF & (idx) )
#define REG_INDEX_DOCUMENT_SECTOR(idx) ( 0x00007FFF & (idx) )
#define REG_INDEX_PHRASE_WEIGHT(idx)   ( (0x30000000 & (idx)) >> 28 )
#define REG_INDEX_PHRASE_POS(idx)      ( (0x7F000000 & (idx)) >> 24 )
#define REG_INDEX_PHRASE_SECTOR(idx)   ( (0x3FFF8000 & (idx)) >> 15 ) 
#define REG_INDEX_PHRASE_OFFSET(idx)   ( 0x0FFFFFFF & (idx) ) 

#define REV_INDEX_DOCUMENT_OFFSET(idx) ( 0x00FFFFFF & (idx) )
#define REV_INDEX_DOCUMENT_SECTOR(idx) ( 0x00007FFF & (idx) )
#define REV_INDEX_PHRASE_WEIGHT(idx)   ( (0x30000000 & (idx)) >> 28 )
#define REV_INDEX_PHRASE_POS(idx)      ( (0x7F000000 & (idx)) >> 24 )
#define REV_INDEX_PHRASE_SECTOR(idx)   ( (0x3FFF8000 & (idx)) >> 15 ) 
#define REV_INDEX_PHRASE_OFFSET(idx)   ( 0x0FFFFFFF & (idx) ) 


#define CREATE_REG_INDEX_HEADER_FIRST(pos, ofs)    ( 0x80000000 | (((pos)  & 0x0000007F) << 24) | ((ofs)  & 0x00FFFFFF)  ) 
#define CREATE_REG_INDEX_HEADER_SECOND(psec, dsec) ( 0xC0000000 | (((psec) & 0x00007FFF) << 15) | ((dsec) & 0x00007FFF) )
#define CREATE_REG_INDEX_BODY_FIRST(wei, ofs)      ( 0x00000000 | (((wei)  & 0x00000003) << 28) | ((ofs)  & 0x0FFFFFFF) )

#define CREATE_REV_INDEX_HEADER_FIRST(wei, ofs)    ( 0x80000000 | (((wei)  & 0x00000003) << 28) | ((ofs)  & 0x0FFFFFFF) )
#define CREATE_REV_INDEX_HEADER_SECOND(psec, dsec) ( 0xC0000000 | (((psec) & 0x00007FFF) << 15) | ((dsec) & 0x00007FFF) )
#define CREATE_REV_INDEX_BODY_FIRST(pos, ofs)      ( 0x00000000 | (((pos)  & 0x0000007F) << 24) | ((ofs)  & 0x00FFFFFF) )


#define NULL_PHRASE     0x80000000
#define NULL_DOCUMENT   0x80000000


#define SEARCH_DEBUG_NONE     0
#define SEARCH_DEBUG_DOCUMENT 1


#define SORT_KEY_COUNT        4

#define ATTR_TYPE_STRING     0x00
#define ATTR_TYPE_INTEGER    0x80

#define ATTR_ROLE_DEFAULT    0
#define ATTR_ROLE_INDEX      1
#define ATTR_ROLE_PKEY       2
#define ATTR_ROLE_FULLTEXT   3

#define IS_ATTR_TYPE_STRING(chr)   ( ((unsigned char)(chr) & 0x80) == ATTR_TYPE_STRING)
#define CREATE_ATTR_HEADER(id, ty)  ( ((ty) & 0x80) | ((id) & 0x7F) )


#define SEARCH_TYPE_MATCH   0
#define SEARCH_TYPE_PREFIX  1
#define SEARCH_TYPE_BETWEEN 2

#define SEARCH_NODE_TYPE_NULL   0
#define SEARCH_NODE_TYPE_AND    1
#define SEARCH_NODE_TYPE_OR     2
#define SEARCH_NODE_TYPE_LEAF   3

#define SEARCH_CACHE_TYPE_NULL     0
#define SEARCH_CACHE_TYPE_EQUAL    1
#define SEARCH_CACHE_TYPE_PREFIX   2
#define SEARCH_CACHE_TYPE_BETWEEN  3


#define EX_APP_APPCONFIG    0
#define EX_APP_MESSAGE      1
#define EX_APP_DOCUMENT     2
#define EX_APP_MORPH        4
#define EX_APP_PHRASE       5
#define EX_APP_REGINDEX     6
#define EX_APP_REVINDEX     7
#define EX_APP_SERVER       8
#define EX_APP_FILE_ACCESS   9
#define EX_APP_SHARED_MEMORY 10
#define EX_APP_INDEXER      11
#define EX_APP_SEARCHER     12
#define EX_APP_UNKNOWN      99

#define INDEX_FLAG_FIN   0
#define INDEX_FLAG_CONT  1




typedef std::vector<struct SearchNode>          SEARCH_NODE_SET;
typedef std::vector<struct SearchHitData>       SEARCH_HIT_DATA_SET;
typedef std::vector<struct SearchCache>         SEARCH_CACHE_SET;
typedef std::vector<struct SearchPartial>  SEARCH_PARTIAL_SET;



// Common type definitions
typedef std::vector<unsigned int>         ID_SET;
typedef std::vector<std::string>          WORD_SET;
typedef std::vector< std::vector<std::string> > SEARCH_WORD_SET;

typedef std::vector<struct ReverseIndex>      REVERSE_INDEX_SET;
typedef std::vector<struct ReverseIndexInfo>  REVERSE_INDEX_INFO_SET;
typedef std::map<unsigned short, REVERSE_INDEX_INFO_SET>  REVERSE_INDEX_INFO_MAP;

typedef std::vector<struct DocumentInfo>      DOCUMENT_INFO_SET;
typedef std::vector<struct DocumentData>      DOCUMENT_DATA_SET;
typedef std::vector<struct DocumentAddr>      DOCUMENT_ADDR_SET;

typedef std::vector<struct RegularIndex>      REGULAR_INDEX_SET;
typedef std::vector<struct RegularIndexInfo>  REGULAR_INDEX_INFO_SET;

typedef std::vector<struct PhraseData>    PHRASE_DATA_SET;
typedef std::vector<struct PhraseData*>   PHRASE_PTR_SET;
typedef std::vector<struct PhraseAddr>    PHRASE_ADDR_SET;
typedef std::vector<struct PhraseInfo>    PHRASE_INFO_SET;

typedef std::vector<struct InsertPhrase>          INSERT_PHRASE_SET;
typedef std::vector<struct InsertReverseIndex>    INSERT_REVERSE_INDEX_SET;
typedef std::vector<struct InsertDocument>        INSERT_DOCUMENT_SET;
typedef std::vector<struct InsertRegularIndex>    INSERT_REGULAR_INDEX_SET;

typedef std::pair<int, int>                   RANGE;
typedef std::vector< std::pair<int, int> >    RANGE_SET;
typedef std::vector<struct MergeData>         MERGE_SET;

typedef std::vector<struct SearchResultRange> SEARCH_RESULT_RANGE_SET;
typedef std::map<std::string, struct AttrDataType>  ATTR_TYPE_MAP;
typedef std::vector<struct AttrDataType> ATTR_TYPE_SET;


extern bool g_debug;


/* Common data structures */
struct AttrDataType {
  unsigned char header;
  bool pkey_flag;
  bool fulltext_flag;
  bool index_flag;
  bool sort_flag;
  bool bit_reverse_flag;
  unsigned char bit_from;
  unsigned char bit_len;
};


struct DocumentData {
  unsigned long id;
  unsigned int  sortkey[SORT_KEY_COUNT];
};

struct DocumentAddr {
  unsigned short sector;
  unsigned int   offset;
};

struct DocumentInfo {
  unsigned int   count;
  int            pageno;
  unsigned char  level;
  DocumentAddr   max_addr;
};

struct InsertDocument {
  DocumentData   data;
  DocumentAddr   addr;
};


struct PhraseData {
  char*  value;
};

struct PhraseAddr {
  unsigned short sector;
  unsigned int   offset;
};

struct PhraseInfo {
  unsigned int  count;
  unsigned int  pageno;
  unsigned char level;
  PhraseAddr   max_addr;
};

struct InsertPhrase {
  unsigned int pos;
  unsigned int weight;

  struct PhraseData data;
  struct PhraseAddr addr;
};


struct ReverseIndex {
  unsigned int val;
};


struct ReverseIndexInfo {
  unsigned int         count;
  unsigned int         pageno;
  unsigned char        level;
  unsigned char        flag;
  ReverseIndex         max[3];
};

struct InsertReverseIndex {
  InsertDocument doc;
  InsertPhrase   phrase;
  bool delete_flag;
};


struct RegularIndex {
  unsigned int val;
};

struct RegularIndexInfo {
  unsigned int count;
  unsigned int pageno;
  unsigned char level;

  DocumentAddr max_addr;
};


struct InsertRegularIndex {
  InsertDocument        doc;
  INSERT_PHRASE_SET     phrases;
};

struct MergeData {
  RANGE src;
  RANGE dst;
};


struct SearchResultRange {
  int pageno;
  int left_offset;
  int right_offset;
};


struct SearchHitData {
  bool          empty;
  unsigned int  id;
  unsigned int  sortkey[SORT_KEY_COUNT];
  unsigned char pos;
};


struct SearchNode {
  int type;
  int cache;

  int left_node;
  int right_node;

  bool pos_check;

  SearchHitData left_hit_cache;
  SearchHitData right_hit_cache;

};



struct SearchCache {
  int   search_type;
  char* phrase1;
  char* phrase2;

  SEARCH_PARTIAL_SET partials;
};

struct SearchPartial {
  int   next_range;
  int   next_hit;

  SEARCH_HIT_DATA_SET        hits;
  SEARCH_RESULT_RANGE_SET    ranges;
};



struct DocumentHeader {
  DocumentInfo root;
  DocumentAddr next_addr;
  unsigned int next_data;
  unsigned int next_info;
};


struct PhraseHeader {
  PhraseInfo   root;
  PhraseAddr   next_addr;
  unsigned int next_data;
  unsigned int next_info;
};


struct ReverseIndexHeader {
  ReverseIndexInfo root;
  unsigned int next_data;
  unsigned int next_info;
};

struct RegularIndexHeader {
  RegularIndexInfo root;
  unsigned int next_data;
  unsigned int next_info;
};


/* Common functions */
int daemonize();

unsigned int chomp(std::string&);
std::string join(const WORD_SET&, const std::string&);
WORD_SET split(const std::string&, const std::string&);


std::string log_level_string(unsigned int log_level);
void        write_log(unsigned int, std::string, std::string = "");





/*  JSON parse */
std::string get_attr_value_string(JsonValue*);
int         get_attr_value_integer(JsonValue*);


// compare
int sector_comp(unsigned short, unsigned short);
int document_addr_comp(DocumentAddr, DocumentAddr);
int phrase_addr_comp(PhraseAddr, PhraseAddr);
int phrase_data_comp(PhraseData, PhraseData);
int reverse_index_comp(InsertReverseIndex, InsertReverseIndex);
int search_hit_data_comp(SearchHitData, SearchHitData);
int search_hit_data_comp_weak(SearchHitData, SearchHitData);

/*  Compare functors */
class InsertReverseIndexComp {
  public:
    bool operator() (InsertReverseIndex a, InsertReverseIndex b) const {
      return reverse_index_comp(a, b) < 0;
    }
};


class InsertDocumentComp {
  public:
    bool operator() (InsertDocument a, InsertDocument b) const {
      return a.data.id < b.data.id;
    }
};

class InsertPhraseComp {
  public:
    bool operator() (InsertPhrase a, InsertPhrase b) const {
      return phrase_data_comp(a.data, b.data) < 0;
    }
};

class InsertPhraseEqual {
  public:
    bool operator() (InsertPhrase a, InsertPhrase b) const {
      return phrase_data_comp(a.data, b.data) ==  0;
    }
};

class InsertRegularIndexComp {
  public:
    bool operator() (InsertRegularIndex a, InsertRegularIndex b) const {
      return document_addr_comp(a.doc.addr, b.doc.addr) < 0; 
    }
};

class InsertRegularIndexEqual {
  public:
    bool operator() (InsertRegularIndex a, InsertRegularIndex b) const {
      return document_addr_comp(a.doc.addr, b.doc.addr) == 0;
    }
};

class SearchHitDataComp {
  public:
    bool operator() (SearchHitData a, SearchHitData b) const {
      return search_hit_data_comp(a, b) < 0;
    }
};


#endif // __COMMON_H__
