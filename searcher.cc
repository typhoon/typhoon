 /*****************************************************************
 *  searcher.cc
 *    $Date:: 2009-04-17 20:42:57 +0900#$
 *    $Author: imamura_shoichi $
 * 
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *******************************************************************/
#include "common.h"
#include "searcher.h"

Searcher::Searcher() {
  setup(".", "", NULL, NULL);
}


Searcher::Searcher(std::string _path, std::string _log_file, ATTR_TYPE_MAP* _attrs, SharedMemoryAccess* _shm) {
  setup(_path, _log_file, _attrs, _shm);
}


Searcher::~Searcher() {
  buf.clear();
  nodes.clear();
  caches.clear();
  order.clear();
  data.finish();
}


void Searcher::init() {
  offset = 0;
  limit  = 10;
  root_node = -1;
  lazy_count = true;

  buf.clear();
  nodes.clear();
  caches.clear();
  order.clear();
}


void Searcher::setup(std::string _path, std::string _log_file, ATTR_TYPE_MAP* _attrs, SharedMemoryAccess* _shm) {
  path = _path;
  log_file = _log_file;
  attrs = _attrs;
  shm = _shm;

  init();
  data.setup(path, shm);
}



bool Searcher::parse_request(JsonValue* request, AppConfig& cfg) {
  if(!request || request->get_value_type() != json_object) {
    return false;
  }

  if(request->get_value_by_tag("offset")) offset = request->get_value_by_tag("offset")->get_integer_value();
  if(request->get_value_by_tag("limit"))  limit  = request->get_value_by_tag("limit")->get_integer_value();
  if(offset < 0) offset = 0;
  if(offset > (int)cfg.max_offset) offset = (int)cfg.max_offset;
  if(limit < 0)  limit = 0;
  if(limit  > (int)cfg.max_limit)  limit = (int)cfg.max_limit;

  JsonValue* val = request->get_value_by_tag("conditions");
  SearchNode n = {SEARCH_NODE_TYPE_OR, -1, -1, -1, false};
  n.left_node  = parse_conditions(val); 
  n.right_node = root_node;
  nodes.push_back(n);
  root_node = nodes.size()-1;

  if(!parse_order(request->get_value_by_tag("order"))) return false;

  return true;  
}

bool Searcher::parse_order(JsonValue* val) {
  if(!val) return true;
  if(val->get_value_type() != json_array) return false;

  for(unsigned int i=0; i<val->get_array_value()->size(); i++) {
    JsonValue* keyname_val = val->get_array_value()->at(i);
    if(keyname_val->get_value_type() != json_string) return false;

    std::string str = keyname_val->get_string_value();
    WORD_SET key_order = split(str, ",");
    if(key_order.size() == 0 || key_order.size() > 2) return false;
    ATTR_TYPE_MAP::iterator it = attrs->find(key_order[0]);
    if(it == attrs->end()) return false;
    AttrDataType attr_type = it->second;
    if(!attr_type.sort_flag) return false;
    attr_type.bit_reverse_flag = attr_type.bit_reverse_flag ^ ((key_order.size() == 2 && key_order[1] == "desc") ? true : false);
    order.push_back(attr_type);
  }

  return true;
}



int Searcher::parse_conditions(JsonValue* val) {
  if(!val) return -1;
  if(val->get_value_type() != json_array) return -1;
  if(val->get_array_value()->size() == 0)  return -1;

  JsonValue* first_node = val->get_value_by_index(0);
  if(!first_node) return -1;
  else if(first_node->get_value_type() == json_string) {
    return parse_conditions_leaf(val);
  }
  else if(first_node->get_value_type() == json_array) {
    int cnt = 0;
    int current = -1;
    while(JsonValue* child = val->get_value_by_index(cnt)) {
      SearchNode n = {SEARCH_NODE_TYPE_AND, -1, -1, -1, false};
      n.left_node  = parse_conditions_level1(child);
      n.right_node = current; 
      nodes.push_back(n);
      current = nodes.size()-1;

      cnt++;
    }

    return current;
  } 

  return -1;
}

int Searcher::parse_conditions_level1(JsonValue* val) {
  if(!val)  return -1;
  if(val->get_value_type() != json_array) return -1;
  if(val->get_array_value()->size() == 0)  return -1;

  JsonValue* first_node = val->get_value_by_index(0);
  if(!first_node) return -1;
  if(first_node->get_value_type() == json_string) {
    return parse_conditions_leaf(val);
  }
  else if(first_node->get_value_type() == json_array) { 
    int cnt = 0;
    int current = -1;

    while(JsonValue* child = val->get_value_by_index(cnt)) {
      SearchNode n = {SEARCH_NODE_TYPE_OR, -1, -1, -1, false};
      n.left_node  = parse_conditions_leaf(child);
      n.right_node = current; 
      nodes.push_back(n);
      current = nodes.size()-1;

      cnt++;
    }
    return current;
  }

  return -1;
}

int Searcher::parse_conditions_leaf(JsonValue* val) {
  if(!val)  return -1;
  if(val->get_value_type() != json_array) return -1;
  if(val->get_array_value()->size() < 3)  return -1;

  JsonValue* first_node = val->get_value_by_index(0);
  if(!first_node || first_node->get_value_type() != json_string) return -1;
  std::string  attr_name = first_node->get_string_value();
  AttrDataType attr_type = {CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING), false, false, false, false, false, 0, 0};
  if(attrs->find(attr_name) != attrs->end()) attr_type = attrs->find(attr_name)->second;

  JsonValue* second_node = val->get_value_by_index(1);
  std::string op = second_node->get_string_value();
  if(attr_type.fulltext_flag && op == "equal") {
    return parse_conditions_fulltext(val->get_value_by_index(2), attr_name, attr_type);
  }

  SearchNode n = {SEARCH_NODE_TYPE_LEAF, -1, -1, -1, false};
  SearchCache c = {SEARCH_CACHE_TYPE_NULL, NULL, NULL};

  if(op == "equal") {
    c.search_type = SEARCH_CACHE_TYPE_EQUAL;
    c.phrase1 = parse_conditions_index(attr_name, attr_type, val->get_value_by_index(2));
  } 
  else if(op == "prefix") {
    c.search_type = SEARCH_CACHE_TYPE_PREFIX;
    c.phrase1 = parse_conditions_index(attr_name, attr_type, val->get_value_by_index(2));
  }
  else if(op == "between") {
    if(val->get_array_value()->size() < 4)  return -1;
    c.search_type = SEARCH_CACHE_TYPE_BETWEEN;
    c.phrase1 = parse_conditions_index(attr_name, attr_type, val->get_value_by_index(2));
    c.phrase2 = parse_conditions_index(attr_name, attr_type, val->get_value_by_index(3));
  }

  caches.push_back(c);
  n.cache = caches.size()-1;
  nodes.push_back(n);

  return nodes.size()-1;
}


int Searcher::parse_conditions_fulltext
(JsonValue* val, std::string attr_name, AttrDataType attr_type) {
  if(!val || val->get_value_type() != json_string)  return -1;
  MorphController m;
  WORD_SET p; 
  std::string word = val->get_string_value();
  m.get_search_phrases(word.c_str(), p, MAX_PHRASE_LENGTH);

  int prev_node = -1;
  bool pos_check = true;
  for(unsigned int i=0; i<p.size(); i++) {
    if(p[i] == "") {
      pos_check = false;
      continue;
    }

    SearchCache c = {SEARCH_CACHE_TYPE_EQUAL, NULL, NULL};
    c.phrase1  = buf.allocate(p[i].length() + 1 + sizeof(unsigned char));
    if(!c.phrase1) continue;
    c.phrase1[0]= attr_type.header;
    strcpy(c.phrase1+1, p[i].c_str());
    caches.push_back(c);

    SearchNode root_node = {SEARCH_NODE_TYPE_AND, -1, -1, prev_node, pos_check};
    SearchNode leaf_node = {SEARCH_NODE_TYPE_LEAF, caches.size()-1, -1, -1, 0};
    nodes.push_back(leaf_node);
    root_node.left_node = nodes.size()-1;
    nodes.push_back(root_node);
    prev_node = nodes.size()-1;
    pos_check = true; 
  }


  return prev_node;
}

char* Searcher::parse_conditions_index(std::string attr_name, AttrDataType attr_type, JsonValue* val) {
  if(!val) return NULL;

  char* ptr = NULL;
  if(!attr_type.index_flag) {
    std::string str = attr_name + "\t" + get_attr_value_string(val);
    ptr = buf.allocate(str.length() + 1 + sizeof(unsigned char));
    if(!ptr) return NULL;
    ptr[0] = attr_type.header;
    strcpy(ptr+1, str.c_str());
  } else if(!IS_ATTR_TYPE_STRING(attr_type.header)) {
    int x = get_attr_value_integer(val);
    ptr = buf.allocate(sizeof(int)+sizeof(unsigned char));
    if(!ptr) return NULL;
    ptr[0] = attr_type.header;
    memcpy(ptr+1, &x, sizeof(int));
  } else if(IS_ATTR_TYPE_STRING(attr_type.header)) {
    std::string str = get_attr_value_string(val);
    ptr = buf.allocate(str.length() + 1 + sizeof(unsigned char));
    if(!ptr) return NULL;
    ptr[0] = attr_type.header;
    strcpy(ptr+1, str.c_str());
  }

  return ptr;
}



int Searcher::do_search(SEARCH_HIT_DATA_SET& result) {
  int hit_count = 0;
  setup_cache();
  setup_node();

  SearchHitData hit_data;
  for(int i=0; i<offset+limit; i++) {
    while(1) {
      hit_data = pickup_hit(root_node);
      if(result.size() == 0 || hit_data.empty || search_hit_data_comp_weak(hit_data, result[result.size()-1]) != 0)  break;
    }
    if(hit_data.empty) break;
    result.push_back(hit_data);
    hit_count++;
  }

  if(lazy_count) {
    int guess_count = 0;
    int total = 0;
    int current = 0;
    for(int i=0; i<(int)caches.size(); i++) {
      for(int j=0; j<(int)caches[i].partials.size(); j++) {
        for(int k=0; k<(int)caches[i].partials[j].ranges.size(); k++) {
          SearchResultRange& r = caches[i].partials[j].ranges[k];
          SearchPartial&     p = caches[i].partials[j];
          total = total + r.right_offset-r.left_offset+1;
          if(k==p.next_range-1 && p.hits.size() > 0) {
            current = current + ((r.right_offset-r.left_offset+1)*p.next_hit)/p.hits.size();  
          } else if(k<p.next_range-1) {
            current = current + (r.right_offset-r.left_offset+1);
          }
        }
      }
    }

    if(current < 0)  current = 0;
    if(current < 20) current = current + 2;

    guess_count = (hit_count * total)/current;
    if(guess_count > 1000) {
      return guess_count;
    }
  }

  SearchHitData prev_hit = hit_data;
  while(1) {
    hit_data = pickup_hit(root_node);
    if(hit_data.empty) break;
    if(search_hit_data_comp_weak(hit_data, prev_hit) != 0) {
      hit_count++;
      prev_hit = hit_data;
    }
  }

  return hit_count; 
}

bool Searcher::search(JsonValue* request_obj, JsonValue* return_obj, AppConfig& cfg) {
  SEARCH_HIT_DATA_SET result;

  try {
    init();
    if(!parse_request(request_obj, cfg)) {
      throw AppException(EX_APP_SEARCHER, "failed to parse request");
    }
    int hit_count = do_search(result);

    return_obj->add_to_object("count", new JsonValue(hit_count));
    return_obj->add_to_object("result", new JsonValue(json_array));
    for(int i=offset; i<(int)result.size() && i<offset+limit; i++) {
      return_obj->get_value_by_tag("result")->add_to_array(new JsonValue((int)result[i].id));
    }
    return_obj->add_to_object("error", new JsonValue(json_null));
  } catch(AppException e) {
    write_log(LOG_LEVEL_ERROR, e.what(), log_file);
    return_obj->add_to_object("count", new JsonValue(0));
    return_obj->add_to_object("result", new JsonValue(json_array));
    return_obj->add_to_object("error", new JsonValue(e.what().c_str()));
  } catch(JsonException e) {
    return_obj->add_to_object("count", new JsonValue(0));
    return_obj->add_to_object("result", new JsonValue(json_array));
    return_obj->add_to_object("error", new JsonValue("JSON access error"));
  }
  data.finish();
  return true;
}


void Searcher::setup_node() {
  for(unsigned int i=0; i<nodes.size(); i++) {
    nodes[i].left_hit_cache.empty = true;
    nodes[i].right_hit_cache.empty = true;
  }
}


void Searcher::setup_cache() {
  for(unsigned int i=0; i<caches.size(); i++) {
    SearchPartial p = {0, 0};

    unsigned short the_sector = data.document_data.get_next_addr().sector;

    if(caches[i].search_type == SEARCH_CACHE_TYPE_EQUAL && order.size() == 0) {
      for(unsigned short s=0; s<=the_sector; s++) {
        caches[i].partials.push_back(p);
        data.reverse_index.find_range(caches[i].phrase1, caches[i].partials[s].ranges, s);
        data.reverse_index.find_hit_data_partial(caches[i].partials[s].hits, caches[i].partials[s].ranges, 0, order);
        caches[i].partials[s].next_range = 1;
      }
    }
    else if(caches[i].search_type == SEARCH_CACHE_TYPE_EQUAL) {
      caches[i].partials.push_back(p);
      for(unsigned short s=0; s<=the_sector; s++) {
        data.reverse_index.find_range(caches[i].phrase1, caches[i].partials[0].ranges, s);
      }
      data.reverse_index.find_hit_data_all(caches[i].partials[0].hits, caches[i].partials[0].ranges, order);

      sort(caches[i].partials[0].hits.begin(), caches[i].partials[0].hits.end(), SearchHitDataComp());
      caches[i].partials[0].next_range = caches[i].partials[0].ranges.size();
    }
    else if(caches[i].search_type == SEARCH_CACHE_TYPE_PREFIX) {
      caches[i].partials.push_back(p);
      for(unsigned short s=0; s<=the_sector; s++) {
        data.reverse_index.find_prefix_range(caches[i].phrase1, caches[i].partials[0].ranges, s);
      }
      data.reverse_index.find_hit_data_all(caches[i].partials[0].hits, caches[i].partials[0].ranges, order);

      sort(caches[i].partials[0].hits.begin(), caches[i].partials[0].hits.end(), SearchHitDataComp());
      caches[i].partials[0].next_range = caches[i].partials[0].ranges.size();
    }
    else if(caches[i].search_type == SEARCH_CACHE_TYPE_BETWEEN) {
      caches[i].partials.push_back(p);
      for(unsigned short s=0; s<=the_sector; s++) {
        data.reverse_index.find_between_range(caches[i].phrase1, caches[i].phrase2, caches[i].partials[0].ranges, s);
      }
      data.reverse_index.find_hit_data_all(caches[i].partials[0].hits, caches[i].partials[0].ranges, order);
      sort(caches[i].partials[0].hits.begin(), caches[i].partials[0].hits.end(), SearchHitDataComp());
      caches[i].partials[0].next_range = caches[i].partials[0].ranges.size();
    }
    else {
      continue;
    }
  }
}



SearchHitData Searcher::pickup_hit(int node_id) {
  SearchHitData empty = {true, 0, {0, 0, 0, 0}, 0};
  if(node_id < 0 || node_id >= (int)nodes.size()) return empty;

  SearchNode& n = nodes[node_id];
  SearchHitData hit_data = empty;
  if(n.type == SEARCH_NODE_TYPE_LEAF) {
    n.left_hit_cache = n.right_hit_cache = pickup_cache(n.cache);
    return n.left_hit_cache;
  } else if(n.left_node == -1) {
    hit_data = pickup_hit(n.right_node);
  } else if(n.right_node == -1) {
    hit_data = pickup_hit(n.left_node);
  } else if(n.type == SEARCH_NODE_TYPE_AND) { 
    if(n.left_hit_cache.empty)  n.left_hit_cache = pickup_hit(n.left_node);
    if(n.right_hit_cache.empty) n.right_hit_cache = pickup_hit(n.right_node);

    while(1) {
      if(n.left_hit_cache.empty || n.right_hit_cache.empty) return empty;

      int cmp = search_hit_data_comp_weak(n.left_hit_cache, n.right_hit_cache);
      if(cmp == 0 && n.pos_check && n.left_hit_cache.pos - n.right_hit_cache.pos != 1) {
        cmp = n.left_hit_cache.pos - n.right_hit_cache.pos;
      }
 
      if(cmp == 0) {
        hit_data = n.left_hit_cache;
        n.left_hit_cache.empty = true;
        n.right_hit_cache.empty = true;
        break;
      } else if(cmp < 0) {
        n.left_hit_cache = pickup_hit(n.left_node);
      } else {
        n.right_hit_cache = pickup_hit(n.right_node);
      }
    }
  } else if(n.type == SEARCH_NODE_TYPE_OR) {
    if(n.left_hit_cache.empty)  n.left_hit_cache = pickup_hit(n.left_node);
    if(n.right_hit_cache.empty) n.right_hit_cache = pickup_hit(n.right_node);

    if(n.left_hit_cache.empty && n.right_hit_cache.empty) return empty;
    int cmp = search_hit_data_comp_weak(n.left_hit_cache, n.right_hit_cache);
    if(cmp == 0) {
      hit_data = n.left_hit_cache;
      n.left_hit_cache.empty = true;
      n.right_hit_cache.empty = true;
    } else if(cmp < 0) {
      hit_data = n.left_hit_cache;
      n.left_hit_cache.empty = true;
    } else {
      hit_data = n.right_hit_cache;
      n.right_hit_cache.empty = true;
    }
  }

  return hit_data;
}


void Searcher::clear_current_hit(int node_id) {
  if(node_id < 0 || node_id >= (int)nodes.size()) return;

  nodes[node_id].left_hit_cache.empty = true;
  nodes[node_id].right_hit_cache.empty = true;
  clear_current_hit(nodes[node_id].left_node);
  clear_current_hit(nodes[node_id].right_node);
}


SearchHitData Searcher::pickup_cache(int cache_id) {
  SearchHitData empty = {true, 0, {0, 0, 0}, 0};
  if(cache_id < 0 || cache_id >= (int)caches.size()) return empty;

  SearchHitData hit_data = empty;
  int pickup_index = -1;
  for(unsigned int i=0; i<caches[cache_id].partials.size(); i++) {
    SearchPartial&  p = caches[cache_id].partials[i];
    if(p.ranges.size() == 0) continue;

    if(p.next_hit >= (int)p.hits.size()) {
      p.hits.clear();
      if(p.next_range >= (int)p.ranges.size()) continue;
      data.reverse_index.find_hit_data_partial(p.hits, p.ranges, p.next_range, order);
      p.next_hit = 0;
      p.next_range++;
    }
    if(p.hits.size() == 0) continue;

    if(search_hit_data_comp(hit_data, p.hits[p.next_hit]) > 0) {
      hit_data = p.hits[p.next_hit];
      pickup_index = i;
    }
  }
  if(pickup_index < 0) return empty;

  caches[cache_id].partials[pickup_index].next_hit++;
  return hit_data;
}



////////////////////////////////////////////////////////////////////
//  for debug
////////////////////////////////////////////////////////////////////
bool Searcher::test() {
  SEARCH_HIT_DATA_SET hits;
  int hit_count;

  AppConfig cfg;
  std::string request_str;
  JsonValue* val = NULL;

  try {
    ATTR_TYPE_MAP::iterator itr = attrs->find("title");
    AttrDataType t = {CREATE_ATTR_HEADER(0, ATTR_TYPE_STRING), false, false, false, false, false, 0, 0};
    if(itr != attrs->end())  t = itr->second;
    std::cout << "initialize...\n";
    data.init();
    std::cout << "setup sample data...\n";
    data.set_sample(t.header);
 
    std::cout << "error request test...\n";
    request_str = "{}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    hits.clear();
    hit_count = do_search(hits);  
    if(hit_count != 0 || hits.size() != 0) throw AppException(EX_APP_SEARCHER, "");
  
   
    std::cout << "no condition request...\n";
    request_str = "{\"offset\":0, \"limit\":10, \"conditions\":[]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    hits.clear();
    hit_count = do_search(hits);  
    if(hit_count != 0 || hits.size() != 0) throw AppException(EX_APP_SEARCHER, "");
  
  
    std::cout << "single condition request...\n";
    request_str = "{\"offset\":0, \"limit\":10, \"conditions\":[\"title\", \"equal\", \"p000099\"]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits);  
    if(hit_count != 3000 || hits.size() != 10) throw AppException(EX_APP_SEARCHER, "");
  
  
    std::cout << "unknown attribute request...\n";
    request_str = "{\"offset\":0, \"limit\":10, \"conditions\":[\"unknown\", \"equal\", \"p000099\"]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
    if(hit_count != 0 || hits.size() != 0) throw AppException(EX_APP_SEARCHER, "");
  
  
    std::cout << "AND-search request...\n";
    request_str = "{\"offset\":0, \"limit\":10, \"conditions\":[[\"title\", \"equal\", \"p000099\"], [\"title\", \"equal\", \"p000000\"], [\"title\", \"equal\", \"p000010\"]]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
    if(hit_count != 1 + 3000/70) throw AppException(EX_APP_SEARCHER, "");
    for(unsigned int i=0; i<hits.size(); i++) {
      if(hits[i].id % 70 != 0) throw AppException(EX_APP_SEARCHER, "");
    }
  
    std::cout << "OR-search request...\n";
    request_str = "{\"offset\":0, \"limit\":3000, \"conditions\":[[[\"title\", \"equal\", \"p000001\"], [\"title\", \"equal\", \"p000002\"], [\"title\", \"equal\", \"p000003\"]]]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
  
    if(hit_count != (3000/10)*3) throw AppException(EX_APP_SEARCHER, "");
    for(unsigned int i=0; i<hits.size(); i++) {
      if(i>0 && hits[i].sortkey[0] < hits[i-1].sortkey[0]) throw AppException(EX_APP_SEARCHER, "");
      if(hits[i].id % 10 != 1 && hits[i].id % 10 != 2 && hits[i].id % 10 != 3) throw AppException(EX_APP_SEARCHER, "");
    }
    
  
    std::cout << "prefix search request...\n";
    request_str = "{\"offset\":0, \"limit\":3000, \"conditions\":[[\"title\", \"prefix\", \"p075\"]]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
    if(hit_count != 10) throw AppException(EX_APP_SEARCHER, "");
    for(unsigned int i=0; i<hits.size(); i++) {
      if(i>0 && hits[i].sortkey[0] < hits[i-1].sortkey[0]) throw AppException(EX_APP_SEARCHER, "");
      if(hits[i].id < 750 || hits[i].id > 760) throw AppException(EX_APP_SEARCHER, "");
    }
  
    std::cout << "range search request...\n";
    request_str = "{\"offset\":0, \"limit\":3000, \"conditions\":[[\"title\", \"between\", \"p035000\", \"p036000\"]]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
    if(hit_count != 10) throw AppException(EX_APP_SEARCHER, "");
    for(unsigned int i=0; i<hits.size(); i++) {
      if(hits[i].id < 350 || hits[i].id > 360) throw AppException(EX_APP_SEARCHER, "");
    }
  
  
    std::cout << "complicated search request...\n";
    request_str = "{\"offset\":0, \"limit\":3000, \"conditions\":[[\"title\", \"equal\", \"p000000\"], [[\"title\", \"equal\", \"p000010\"], [\"title\", \"equal\", \"p000011\"], [\"title\", \"equal\", \"p000012\"]]]}";
    val = JsonImport::json_import(request_str); 
    init();
    parse_request(val, cfg);
    delete val;
    lazy_count = false;
    hits.clear();
    hit_count = do_search(hits); 
    for(unsigned int i=0; i<hits.size(); i++) {
      if(hits[i].id % 10 != 0 || (hits[i].id % 7 != 0 && hits[i].id % 7 != 1 && hits[i].id % 7 != 2)) throw AppException(EX_APP_SEARCHER, "");
    }
    
    std::cout << "ordered request...\n";
    request_str = "{\"offset\":0, \"limit\":10, \"conditions\":[\"title\", \"equal\", \"p000099\"], \"order\":[\"rank\"]}";
    val = JsonImport::json_import(request_str); 
    init();
    if(!parse_request(val, cfg)) throw AppException(EX_APP_SEARCHER, "");
    delete val;
  } catch(AppException e) {
    if(val) delete val;
    return false;
  }

  buf.clear(); 
  return true;
}



void Searcher::dump() {
  std::cout << "search query status...\n";
  std::cout << "  [offset]" << offset << "\n";
  std::cout << "  [limit]" << limit << "\n";
  std::cout << "  [search node]" << root_node << "\n";
  for(unsigned int i=0; i<nodes.size(); i++) {
    std::cout << "    " << i << ":";
    if(nodes[i].type == SEARCH_NODE_TYPE_NULL)      std::cout << "[NULL]";
    else if(nodes[i].type == SEARCH_NODE_TYPE_AND)  std::cout << "[AND]";
    else if(nodes[i].type == SEARCH_NODE_TYPE_OR)   std::cout << "[OR]";
    else if(nodes[i].type == SEARCH_NODE_TYPE_LEAF) std::cout << "[LEAF]";
    else                                              std::cout << "[UNKNOWN]";

    if(nodes[i].cache != -1) {
      SearchCache c = caches[nodes[i].cache];

      if(c.search_type == SEARCH_CACHE_TYPE_NULL)         std::cout << "(null)";
      else if(c.search_type == SEARCH_CACHE_TYPE_EQUAL)   std::cout << "(equal)";
      else if(c.search_type == SEARCH_CACHE_TYPE_PREFIX)  std::cout << "(prefix)";
      else if(c.search_type == SEARCH_CACHE_TYPE_BETWEEN) std::cout << "(between)";
      else                                                std::cout << "(unknown)";

      if(!c.phrase1)                             std::cout << "{null}";
      else if(IS_ATTR_TYPE_STRING(c.phrase1[0])) std::cout << c.phrase1+1;
      else                                       std::cout << *((int*)(c.phrase1+1));
   
      std::cout << ",";

      if(!c.phrase2)                             std::cout << "{null}";
      else if(IS_ATTR_TYPE_STRING(c.phrase1[0])) std::cout << c.phrase2+1;
      else                                       std::cout << *((int*)(c.phrase2+1));
     
     std::cout << "   ";
    }

    std::cout << nodes[i].left_node << "," << nodes[i].right_node << (nodes[i].pos_check ? "*" : "") << "\n";
  }

}
