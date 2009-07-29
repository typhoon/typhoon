/*****************************************************************
 *  index.cc
 *    brief:
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 19:53:02 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#include "common.h"
#include "exception.h"
#include "server.h"
#include "indexer.h"


//////////////////////////////////////////////////////////////
//  constructor 
//////////////////////////////////////////////////////////////
Indexer::Indexer() {
  path = ".";
  log_file = "";
  shm = NULL;
}

Indexer::~Indexer() {
  data.finish();
}

Indexer::Indexer
(std::string _path, std::string _log_file, ATTR_TYPE_MAP* _attrs, SharedMemoryAccess* _shm, MorphController* _morph, Buffer* _buf) {
  setup(_path, _log_file, _attrs, _shm, _morph, _buf); 
} 


//////////////////////////////////////////////////////////////
//  setting 
//////////////////////////////////////////////////////////////
void Indexer::setup
(std::string _path, std::string _log_file, ATTR_TYPE_MAP* _attrs, SharedMemoryAccess* _shm, MorphController* _morph, Buffer* _buf) {
  path = _path;
  log_file = _log_file;
  attrs = _attrs;
  shm = _shm;
  morph = _morph;
  buf = _buf;

  data.setup(path, shm);
}






//////////////////////////////////////////////////////////////
//  main index procedure
//////////////////////////////////////////////////////////////
bool Indexer::proc_remove_indexes(INSERT_REGULAR_INDEX_SET& reg_idx) {
  DocumentController& dc = data.document;
  RegularIndexController& regc = data.regular_index;
  PhraseController& pc = data.phrase;
  ReverseIndexController& revc = data.reverse_index;

  INSERT_REGULAR_INDEX_SET old_idx;
  for(unsigned int i=0; i<reg_idx.size(); i++) {
    InsertRegularIndex reg;
    reg.doc.addr = dc.find_addr(reg_idx[i].doc.data.id);
    if(reg.doc.addr.offset != NULL_DOCUMENT) {
      reg.doc.data = dc.find_by_addr(reg.doc.addr);
      old_idx.push_back(reg);
    }
  }

  for(unsigned int i=0; i<old_idx.size(); i++) {
    regc.remove(old_idx[i]);
    for(unsigned int j=0; j<old_idx[i].phrases.size(); j++) {
      old_idx[i].phrases[j].data = pc.find_data(old_idx[i].phrases[j].addr, *buf); 
    }
    set_phrase_pos(old_idx[i].phrases);
  }


  INSERT_REVERSE_INDEX_SET old_rev_idx;
  for(unsigned int i=0; i<old_idx.size(); i++) {
    for(unsigned int j=0; j<old_idx[i].phrases.size(); j++) {
      InsertReverseIndex rev = {old_idx[i].doc, old_idx[i].phrases[j], true};
      old_rev_idx.push_back(rev);
    }
  }


  sort(old_rev_idx.begin(), old_rev_idx.end(), InsertReverseIndexComp());
  revc.insert(old_rev_idx); 
  data.finish(); 
  return true;
}


bool Indexer::proc_insert_documents(INSERT_REGULAR_INDEX_SET& reg_idx) {
  DocumentController& dc = data.document;
  INSERT_DOCUMENT_SET docs;
  for(unsigned int i=0; i<reg_idx.size(); i++) {
    InsertDocument ins = {{}, {0, NULL_DOCUMENT}}; 
    ins.data = reg_idx[i].doc.data;
    docs.push_back(ins);
  }
  sort(docs.begin(), docs.end(), InsertDocumentComp());

  if(!dc.insert(docs)) return false;

  // reflect
  for(unsigned int i=0; i<reg_idx.size(); i++) {
    reg_idx[i].doc.addr = find_document_addr_by_cache(reg_idx[i].doc.data, docs); 
  }
  data.finish(); 
 
  return true;
}

bool Indexer::proc_insert_phrases(INSERT_REGULAR_INDEX_SET& reg_idx) {
  PhraseController& pc = data.phrase;
  INSERT_PHRASE_SET phrase_set;

  for(unsigned int i=0; i<reg_idx.size(); i++) {
    for(unsigned int j=0; j<reg_idx[i].phrases.size(); j++) {
      phrase_set.push_back(reg_idx[i].phrases[j]);
    }
  }
  sort(phrase_set.begin(), phrase_set.end(), InsertPhraseComp()); 
  phrase_set.erase(unique(phrase_set.begin(), phrase_set.end(), InsertPhraseEqual()), phrase_set.end());


  pc.insert(phrase_set);
  for(unsigned int i=0; i<reg_idx.size(); i++) {
    for(unsigned int j=0; j<reg_idx[i].phrases.size(); j++) {
      reg_idx[i].phrases[j].addr = find_phrase_addr_by_cache(reg_idx[i].phrases[j].data, phrase_set);
    } 
  }
  data.finish(); 
  return true;
}


bool Indexer::proc_insert_regular_indexes(INSERT_REGULAR_INDEX_SET& reg_index) {
  RegularIndexController& regc = data.regular_index;

  stable_sort(reg_index.begin(), reg_index.end(), InsertRegularIndexComp());
  reg_index.erase(unique(reg_index.begin(), reg_index.end(), InsertRegularIndexEqual()), reg_index.end()); 
  if(!regc.insert(reg_index)) return false;
  data.finish(); 

  return true;
}

bool Indexer::proc_insert_reverse_indexes(INSERT_REGULAR_INDEX_SET& reg_index) {
  ReverseIndexController& revc = data.reverse_index;
  INSERT_REVERSE_INDEX_SET rev_index;

  for(unsigned int i=0; i<reg_index.size(); i++) {
    for(unsigned int j=0; j<reg_index[i].phrases.size(); j++) {
      InsertReverseIndex rev = {reg_index[i].doc, reg_index[i].phrases[j], false};
      rev_index.push_back(rev);
    }
  }
  sort(rev_index.begin(), rev_index.end(), InsertReverseIndexComp());

  if(!revc.insert(rev_index)) return false;
  data.finish();
  
  return true;
}


bool Indexer::proc_sector_check() {
  int idx_page = shm->get_header()->rev_header.next_data;
  int block_size = shm->get_block_size();
  unsigned short sector = data.document_data.get_next_addr().sector;
 
  if(idx_page/block_size > sector) {
    data.document_data.set_next_sector();
  } 

  return true;
}



PhraseAddr Indexer::find_phrase_addr_by_cache(PhraseData d, INSERT_PHRASE_SET& cache) {
  PhraseAddr err = {0, NULL_PHRASE};

  int l=-1, r=cache.size();
  while(r-l > 1) {
    int mid = (r+l)/2;

    int cmp = phrase_data_comp(cache[mid].data, d);
    if(cmp == 0)     return cache[mid].addr;
    else if(cmp < 0) l = mid;
    else             r = mid;
  }

  return err;
}

DocumentAddr Indexer::find_document_addr_by_cache(DocumentData d, INSERT_DOCUMENT_SET& cache) {
  DocumentAddr err = {0, NULL_DOCUMENT};

  int l=-1, r=cache.size();
  while(r-l > 1) {
    int mid = (r+l)/2;
    if(cache[mid].data.id == d.id)       return cache[mid].addr;
    else if(cache[mid].data.id < d.id)   l = mid;
    else                          r = mid;
  }

  return err;
}



// JSON format request
bool Indexer::parse_request(InsertRegularIndex& idx, JsonValue* request_obj) {
  if(!request_obj) return false;
  bool pkey_exists = false;   

  try {
    WORD_SET tags = request_obj->get_tags();

    // initialize
    for(unsigned int i=0; i<SORT_KEY_COUNT; i++) {
      idx.doc.data.sortkey[i] = 0;
    }

    for(unsigned int i=0; i<tags.size(); i++) {
      ATTR_TYPE_MAP::iterator itr = attrs->find(tags[i]);
      if(itr == attrs->end())  {
        set_default_phrase(idx.phrases, tags[i], request_obj->get_value_by_tag(tags[i]));
      } else {
        AttrDataType t = itr->second;
        if(t.pkey_flag) {
          idx.doc.data.id = get_integer_attr(request_obj->get_value_by_tag(tags[i]));
          pkey_exists = true;
        }
        if(t.sort_flag) {
          int base = get_integer_attr(request_obj->get_value_by_tag(tags[i]));
          if(t.bit_reverse_flag) {
            base = base ^ 0xFFFFFFFF; // desc/asc switch
          }
          base = base << (32-t.bit_len);    // bitmask

          int k = t.bit_from >> 5;
          int shift_bit    =  t.bit_from & 0x1F;
          int first_mask   =  base >> shift_bit;
          idx.doc.data.sortkey[k] |= first_mask;

          if(shift_bit + t.bit_len > 32) {
            int second_mask  =  base << (32-shift_bit);
            if(second_mask != 0) idx.doc.data.sortkey[k+1] |= second_mask;
          }
        }
        if(t.index_flag) { 
          if(t.fulltext_flag) {
            set_fulltext_phrase(idx.phrases, t, tags[i], request_obj->get_value_by_tag(tags[i]));
          } else {
            set_attr_phrase(idx.phrases, t, tags[i], request_obj->get_value_by_tag(tags[i]));
          }
        } 
       }
    }
  } catch(...) {
    return false;
  }

  set_phrase_pos(idx.phrases);
  return pkey_exists;
}



// batch indexer
bool Indexer::do_index(INSERT_REGULAR_INDEX_SET& reg_index) {
  try {
    if(!proc_remove_indexes(reg_index))   throw AppException(EX_APP_INDEXER, "remove index error");
    if(!proc_insert_documents(reg_index)) throw AppException(EX_APP_INDEXER, "set document error");
    if(!proc_insert_phrases(reg_index))   throw  AppException(EX_APP_INDEXER, "set phrase error");
    if(!proc_insert_regular_indexes(reg_index)) throw AppException(EX_APP_INDEXER, "set regular index error");
    if(!proc_insert_reverse_indexes(reg_index)) throw AppException(EX_APP_INDEXER, "set reverse index error");
  } catch(AppException e) {
    write_log(LOG_LEVEL_ERROR, e.what(), log_file);
    data.finish();
    return false;
  }

  return true;
}



// attr_name is not registered.
void Indexer::set_default_phrase
(INSERT_PHRASE_SET& phrases, std::string& attr_name, JsonValue* attr_val) {
  if(!attr_val) return;

  unsigned char header = CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING);
  if(attr_val->get_value_type() == json_array) {
    JsonArray* val = attr_val->get_array_value();
    for(unsigned int i=0; i<val->size(); i++) {
      set_default_phrase(phrases, attr_name, val->at(i));
    }
  } else {
    std::string s = get_string_attr(attr_val);
    s = attr_name + "\t" + s;
    char* phrase_data = get_phrase_data(header, s.c_str(), s.length()+1);
    if(phrase_data) {
      InsertPhrase p = {0, 0, {phrase_data}, {0, NULL_PHRASE}};
      phrases.push_back(p);
    }
  }
}


void Indexer::set_attr_phrase
(INSERT_PHRASE_SET& phrases, AttrDataType attr_type, std::string& attr_name, JsonValue* attr_val) {
  if(!attr_val) return;

  if(attr_val->get_value_type() == json_array) {
    JsonArray* val = attr_val->get_array_value();
    for(unsigned int i=0; i<val->size(); i++) {
      set_attr_phrase(phrases, attr_type, attr_name, val->at(i));
    }
  } else {
    char* phrase_data = NULL;

    if(IS_ATTR_TYPE_STRING(attr_type.header)) {
      std::string s = get_string_attr(attr_val);
      phrase_data = get_phrase_data(attr_type.header, s.c_str(), s.length()+1);
    } else {
      int i = get_integer_attr(attr_val);
      phrase_data = get_phrase_data(attr_type.header, &i, sizeof(int));
    }

    if(phrase_data) {
      InsertPhrase p = {0, 0, {phrase_data}, {0, NULL_PHRASE}};
      phrases.push_back(p);
    }
  }
}


std::string Indexer::get_string_attr(JsonValue* attr_val) {
  if(attr_val->get_value_type() == json_array) {
    return "array";
  } else if(attr_val->get_value_type() == json_object) {
    return "object";
  } else if(attr_val->is_null()) {
    return "null";
  } else if(attr_val->is_true()) {
    return "true";
  } else if(attr_val->is_false()) {
    return "false";
  } else if(attr_val->get_value_type() == json_string) {
    return attr_val->get_string_value();
  } else if(attr_val->get_value_type() == json_integer) {
    char s[20];
    sprintf(s, "%10d", attr_val->get_integer_value()); 
    return std::string(s);
  } else if(attr_val->get_value_type() == json_float) {
    char s[20];
    sprintf(s, "%10f", attr_val->get_float_value()); 
    return std::string(s);
  }

  return "";
}

int Indexer::get_integer_attr(JsonValue* attr_val){
  if(attr_val->get_value_type() == json_array) {
    return 0;
  } else if(attr_val->get_value_type() == json_object) {
    return 0;
  } else if(attr_val->is_null()) {
    return 0;
  } else if(attr_val->is_true()) {
    return 1;
  } else if(attr_val->is_false()) {
    return 0;
  } else if(attr_val->get_value_type() == json_string) {
    std::string s = attr_val->get_string_value();
    return atoi(s.c_str());
  } else if(attr_val->get_value_type() == json_integer) {
    return attr_val->get_integer_value();
  } else if(attr_val->get_value_type() == json_float) {
    return (int)attr_val->get_float_value();
  }

  return 0;
}


char* Indexer::get_phrase_data(unsigned char header, const void* val, unsigned int length) {
  char* ptr = buf->allocate(length + sizeof(unsigned char));
  if(!ptr) return NULL;
  ptr[0] = header;
  memcpy(ptr+1, val, length);

  return ptr;
}

void Indexer::set_fulltext_phrase
(INSERT_PHRASE_SET& phrases, AttrDataType attr_type, std::string& attr_name, JsonValue* attr_val) {
  char* input = (char*)attr_val->get_string_value();

  // get phrases
  int start_pos = 0;
  int the_pos = 0;
  while(1) {
    if(input[the_pos] == '\0') {
      if(the_pos - start_pos > INPUT_LINE_BUFFER_MAX) {
        write_log(LOG_LEVEL_WARNING, "Too long line exists, ignored.", log_file);
      } else if(the_pos - start_pos > 0) {
        set_content_line(phrases, input+start_pos, attr_type, attr_name);
      }
      break;
    } else if(input[the_pos] == '\r' || input[the_pos] == '\n') {
      if(the_pos - start_pos > INPUT_LINE_BUFFER_MAX) {
        write_log(LOG_LEVEL_WARNING, "Too long line exists, ignored.", log_file);
      }
      else if(the_pos - start_pos > INPUT_LINE_BUFFER_SIZE) {
        input[the_pos] = '\0';
        set_content_line(phrases, input+start_pos, attr_type, attr_name);
        start_pos = the_pos+1; 
      }
    }

    the_pos++;
  }

}

void Indexer::set_phrase_pos(INSERT_PHRASE_SET& phrases) {
  // set position
  RANGE_SET r_set;
  for(ATTR_TYPE_MAP::iterator it=attrs->begin(); it!=attrs->end(); it++) { 
    if(!it->second.fulltext_flag) continue;

    RANGE r(-1, -1);
    for(unsigned int i=0; i<phrases.size(); i++) {
      if((unsigned char)(it->second.header) == (unsigned char)phrases[i].data.value[0]) {
        if(r.first == -1) r.first = i;
        r.second = i;
      } else {
        if(r.first != -1) break;
      }
    }

    if(r.first != -1) r_set.push_back(r);
  }

  for(unsigned int i=0; i<r_set.size(); i++) {
    unsigned int word_block = 1 + (r_set[i].second-r_set[i].first+1) / 128;
    for(int p=0; r_set[i].first+p <= r_set[i].second; p++) {
      phrases[p].pos = p/word_block;
    }
  }
}

void Indexer::set_content_line
(INSERT_PHRASE_SET& phrases, char* input, AttrDataType attr_type, std::string& attr_name) {
  MORPH_DATA_SET m;
  morph->get_morph_data(input, m);

  for(unsigned int i=0; i<m.size(); i++)  {
    if(m[i].ignore || m[i].length > MAX_PHRASE_LENGTH) continue;

    unsigned int len = m[i].length + 1 + sizeof(char);
    char* ptr = buf->allocate(len);
    ptr[0] = attr_type.header;
    strncpy(ptr+1, m[i].surface, m[i].length);
    ptr[len-1] = '\0';

    InsertPhrase p = {0, 0, {ptr}, {0, NULL_PHRASE}};
    phrases.push_back(p);
  }
}


std::string Indexer::get_timer_suffix() {
  char suf[20];
  time_t t = time(NULL);
  sprintf(suf, "%015ld", t);

  return std::string(suf);
}


std::string Indexer::get_source_data_file(std::string base_file_str) {
  FileAccess base_file(base_file_str);
  if(!base_file.is_file()) return "";
  WORD_SET suffix_files = base_file.get_suffix_files();
  sort(suffix_files.begin(), suffix_files.end());
  if(suffix_files.size() > 0) return suffix_files[0]; 

  return base_file_str; 
}


/////////////////////////////////////////////////////////
//  debug
/////////////////////////////////////////////////////////
bool Indexer::test() {
  InsertRegularIndex idx;
  std::string request_str;
  JsonValue*  request = NULL;

  try {
    std::cout << "error request test...\n";
    request_str = "{}";
    request = JsonImport::json_import(request_str);
    if(parse_request(idx, request)) throw AppException(EX_APP_INDEXER, "test failed");
    delete request;
    request = NULL;

    request_str = "{\"content\":\"hoge\", \"attr\":\"fuga\"}"; // id not exists
    request = JsonImport::json_import(request_str);
    if(parse_request(idx, request)) throw AppException(EX_APP_INDEXER, "test failed");
    delete request;
    request = NULL;


    std::cout << "parse request test...\n";
    idx.phrases.clear();
    request_str = "{\"id\":10, \"rank\":55, \"content\":\"aaa bbb\", \"title\":\"xxx yyy\", \"user\":\"imasho\", \"age\":32}";
    request = JsonImport::json_import(request_str);
    if(!parse_request(idx, request)) throw AppException(EX_APP_INDEXER, "test failed");
    delete request;
    request = NULL;

    if(idx.doc.data.id != 10 || idx.doc.data.sortkey[0] != (0xFFFFFFFF ^ 55) || idx.phrases.size() != 6) return false;
/*
    if(IS_ATTR_TYPE_STRING(idx.phrases[0].data.value[0]) || *((int*)(idx.phrases[0].data.value+1)) != 32 || 
        idx.phrases[0].pos != 0)  throw AppException(EX_APP_INDEXER, "test failed");
    if(!IS_ATTR_TYPE_STRING(idx.phrases[1].data.value[0]) || strcmp(idx.phrases[1].data.value+1, "aaa") != 0 || 
        idx.phrases[1].pos != 0)  throw AppException(EX_APP_INDEXER, "test failed");
    if(!IS_ATTR_TYPE_STRING(idx.phrases[2].data.value[0]) || strcmp(idx.phrases[2].data.value+1, "bbb") != 0 || 
        idx.phrases[2].pos != 1)  throw AppException(EX_APP_INDEXER, "test failed");
    if(!IS_ATTR_TYPE_STRING(idx.phrases[3].data.value[0]) || strcmp(idx.phrases[3].data.value+1, "xxx") != 0 || 
        idx.phrases[3].pos != 0)  throw AppException(EX_APP_INDEXER, "test failed");
    if(!IS_ATTR_TYPE_STRING(idx.phrases[4].data.value[0]) || strcmp(idx.phrases[4].data.value+1, "yyy") != 0 || 
        idx.phrases[4].pos != 1)  throw AppException(EX_APP_INDEXER, "test failed");
    if(!IS_ATTR_TYPE_STRING(idx.phrases[5].data.value[0]) || strcmp(idx.phrases[5].data.value+1, "user\timasho") != 0 || 
        idx.phrases[5].pos != 0)  throw AppException(EX_APP_INDEXER, "test failed");
*/
    if(idx.phrases[0].data.value[0] == idx.phrases[1].data.value[0]) throw AppException(EX_APP_INDEXER, "test failed");
    if(idx.phrases[1].data.value[0] == idx.phrases[3].data.value[0]) throw AppException(EX_APP_INDEXER, "test failed");
    if(idx.phrases[1].data.value[0] != idx.phrases[2].data.value[0]) throw AppException(EX_APP_INDEXER, "test failed");


    // add index test
    std::cout << "add index test...\n";
    INSERT_REGULAR_INDEX_SET idx_set;
    idx_set.push_back(idx);
    do_index(idx_set);

    DocumentAddr d_adr = data.document.find_addr(idx.doc.data.id);
    if(d_adr.offset == NULL_DOCUMENT) throw AppException(EX_APP_INDEXER, "test failed");
    PhraseAddr   p_adr = data.phrase.find_addr(idx.phrases[0].data);
    if(p_adr.offset == NULL_PHRASE) throw AppException(EX_APP_INDEXER, "test failed");
    p_adr = data.phrase.find_addr(idx.phrases[1].data);
    if(p_adr.offset == NULL_PHRASE) throw AppException(EX_APP_INDEXER, "test failed");

    PHRASE_ADDR_SET p_addrs;
    data.regular_index.find(idx_set[0].doc.addr, p_addrs);
    if(p_addrs.size() != 6) throw AppException(EX_APP_INDEXER, "test failed");


    ID_SET result;
    for(unsigned int i=0; i<idx.phrases.size(); i++) {
      result.clear();
      data.reverse_index.find(idx.phrases[i].data.value, result);
      if(result.size() != 1 || result[0] != 10) throw AppException(EX_APP_INDEXER, "test failed");
    }


    std::cout << "another request test...\n";
    request_str = "{\"id\":11, \"rank\":85, \"content\":\"ccc bbb\", \"title\":\"xxx yyy zzz\", \"user\":\"imamura\", \"age\":30}";
    request = JsonImport::json_import(request_str);
    InsertRegularIndex idx2;
    if(!parse_request(idx2, request)) throw AppException(EX_APP_INDEXER, "test failed");
    delete request;
    request = NULL;

    idx_set.clear();
    idx_set.push_back(idx2);
    do_index(idx_set);
    do_index(idx_set);    


    std::cout << "huge data insert test...\n";
    shm->init();
    data.init();

    WORD_SET words;
    words.push_back("aaa");
    words.push_back("bbb");
    words.push_back("ccc");
    words.push_back("ddd");
    words.push_back("eee");
    words.push_back("fff");
    words.push_back("ggg");
    words.push_back("hhh");
    words.push_back("iii");
  
    for(unsigned int i=0; i<1000; i++) {
      char numstr[30];
  
      sprintf(numstr, "%d", i+1);
      request_str = "";
      request_str = request_str + "{\"id\":"+ numstr + ", \"rank\":85, \"content\":\"";
      request_str = request_str + words[rand()%6] + words[rand()%6] + " " + words[rand()%6];
      request_str = request_str + "\", \"title\":\"";
      request_str = request_str + words[rand()%6] + " " + words[rand()%6] + words[rand()%6] + words[rand()%6];
      sprintf(numstr, "%d", i%30);
      request_str = request_str + "\", \"user\":\"imamura\", \"age\":" + numstr + "}";
  
      request = JsonImport::json_import(request_str);
      InsertRegularIndex idx3;
      parse_request(idx3, request); 
      delete request;
      request = NULL;
  
      idx_set.clear();
      idx_set.push_back(idx3);
      do_index(idx_set);
    }
  
    std::cout << "remove and update test...\n";
    request_str = "{\"id\":777, \"rank\":80, \"content\":\"hoge\", \"user\":\"other\", \"age\":20}";
    request = JsonImport::json_import(request_str);
    InsertRegularIndex idx4;
    parse_request(idx4, request);
    delete request;
    request = NULL;
    idx_set.clear();
    idx_set.push_back(idx4);
    do_index(idx_set);
  
  } catch(AppException e) {
    if(request) delete request;
    return false;
  }

  return true;
}

