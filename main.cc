#include "common.h"
#include "server.h"
#include "indexer.h"
#include "searcher.h"

// global
AppConfig          cfg;
SharedMemoryAccess shm(0, 0);
MorphController    morph;

INSERT_REGULAR_INDEX_SET cache;
Buffer                   common_buf;

bool  get_options(int, char* const);

std::string app_request_handler(const char*, pthread_mutex_t*, int);
void  do_indexer_request(JsonValue*, JsonValue*, pthread_mutex_t*, int);
void  do_searcher_request(JsonValue*, JsonValue*, pthread_mutex_t*, int);

bool  exec_check(void);
bool  app_wait(void);
void  app_exit(int);
void  set_default_signal();
void  set_child_signal();
bool  format();


bool get_options(int argc, char* const argv[]) {
  // initialize
  char* path_buf = new char[MAX_PATH_LENGTH+1];
  getcwd(path_buf, MAX_PATH_LENGTH);
  cfg.path = std::string(path_buf) + "/data";
  cfg.log_file = "";
  cfg.data_file = "";
  cfg.daemon = false;
  cfg.block_size = MEDIUM_MEMORY_BLOCK;

  delete[] path_buf;
  cfg.port = 9999;


  // option setting
  char optchar;
  opterr = 0;
  while((optchar=getopt(argc, argv, "dD:L:p:P:F:o:l:w:a:v")) != -1) {
    if(optchar == 'd') {
      cfg.daemon = true;
      if(cfg.log_file == "") cfg.log_file = std::string(path_buf) + "/log/typhoon.log";
    }
    else if(optchar == 'D') cfg.path = std::string(optarg);
    else if(optchar == 'L') cfg.log_file = std::string(optarg);
    else if(optchar == 'p') cfg.port = (unsigned short)atoi(optarg);
    else if(optchar == 'P') {
      if(!optarg) {cfg.pid_file = "/var/run/typhoon.pid";}
      else {cfg.pid_file = std::string(optarg);}
    }
    else if(optchar == 'o') {cfg.max_offset = (unsigned int)atoi(optarg);}
    else if(optchar == 'l') {cfg.max_limit  = (unsigned int)atoi(optarg);}
    else if(optchar == 'w') {cfg.max_words  = (unsigned int)atoi(optarg);}
    else if(optchar == 'F') {cfg.data_file = std::string(optarg);}
    else if(optchar == 'v') {
      std::cout << "Typhoon version 0.1\n";
      exit(0);
    }
    else {
      return false;
    }
  }

  return true;
}




std::string app_request_handler(const char* request_str, pthread_mutex_t* mutex, int flags) {
  JsonValue* request = request_str ? JsonImport::json_import(request_str) : NULL;
  JsonValue* reply = new JsonValue(json_object);
  JsonValue* cmd_val = request ? request->get_value_by_tag("command") : NULL;
  std::string command = (cmd_val && cmd_val->get_value_type()==json_string) ? cmd_val->get_string_value() : "";

  if(command=="search") { 
    do_searcher_request(request, reply, mutex, flags);
    shm.next_generation();
  } else if(command=="index") {
    JsonValue* data_val = request ? request->get_value_by_tag("data") : NULL;
    do_indexer_request(data_val, reply, mutex, flags);
    shm.next_generation();
  } else {
    std::string msg = "Invalid command: " + command;
    reply->add_to_object("error", new JsonValue(json_true));
    reply->add_to_object("message", new JsonValue(msg));
  } 

  std::string return_str = JsonExport::json_export(reply) + "\n";
  if(reply)  delete reply;
  if(request) delete request;

  return return_str;
}


void do_indexer_request(JsonValue* request, JsonValue* reply, pthread_mutex_t* mutex, int flags) {
   Indexer* i = NULL;
   try {
     i = new Indexer(cfg.path, cfg.log_file, &cfg.attrs, &shm, &morph, &common_buf);

     if(mutex) pthread_mutex_lock(mutex+MUTEX_PARSER);
     InsertRegularIndex idx;
     if(request) {
       if(!i->parse_request(idx, request))  throw AppException(EX_APP_INDEXER, "request parse failed");
     }
     if(mutex) pthread_mutex_unlock(mutex+MUTEX_PARSER);

     if(mutex) pthread_mutex_lock(mutex+MUTEX_INDEXER_PROC1);
     if(request) {
       cache.push_back(idx);
     }
     if((cache.size() > 0 && flags == INDEX_FLAG_FIN) || cache.size() > MAX_DOCUMENT_CACHE) {
       i->proc_remove_indexes(cache);
       i->proc_insert_documents(cache);
       i->proc_insert_phrases(cache);
       i->proc_insert_regular_indexes(cache);
       i->proc_insert_reverse_indexes(cache);
       i->proc_sector_check(); 

       cache.clear();
       common_buf.clear();
     }
     if(mutex) pthread_mutex_unlock(mutex+MUTEX_INDEXER_PROC1);

     delete i;

     reply->add_to_object("error", new JsonValue(json_false));
     reply->add_to_object("message", new JsonValue("Success indexing document"));
  } catch(AppException e) {
    if(mutex) {
      pthread_mutex_unlock(mutex+MUTEX_PARSER);
      pthread_mutex_unlock(mutex+MUTEX_INDEXER_PROC1);
    }
    if(i) delete i;
    cache.clear();
    common_buf.clear();

    std::cout << e.what() << "\n";
    reply->add_to_object("error", new JsonValue(json_true));
    reply->add_to_object("message", new JsonValue(e.what()));
  }

}


void do_searcher_request(JsonValue* request, JsonValue* reply, pthread_mutex_t* mutex, int flags) {
  Searcher* s = new Searcher(cfg.path, cfg.log_file, &cfg.attrs, &shm);

  try {
    if(mutex) pthread_mutex_lock(mutex+MUTEX_PARSER);
    if(!s->parse_request(request, cfg)) throw AppException(EX_APP_SEARCHER, "failed to parse request");
    if(mutex) pthread_mutex_unlock(mutex+MUTEX_PARSER);

    SEARCH_HIT_DATA_SET result;
    int hit_count = s->do_search(result);

    reply->add_to_object("count", new JsonValue(hit_count));
    reply->add_to_object("result", new JsonValue(json_array));
    for(int i=s->offset; i<(int)result.size() && i<s->offset+s->limit; i++) {
      reply->get_value_by_tag("result")->add_to_array(new JsonValue((int)result[i].id));
    }
    reply->add_to_object("error", new JsonValue(json_null));
  } catch(AppException e) {
    if(mutex) pthread_mutex_unlock(mutex+MUTEX_PARSER);
    write_log(LOG_LEVEL_ERROR, e.what(), cfg.log_file);
    reply->add_to_object("count", new JsonValue(0));
    reply->add_to_object("result", new JsonValue(json_array));
    reply->add_to_object("error", new JsonValue(e.what().c_str()));
  } catch(JsonException e) {
    if(mutex) pthread_mutex_unlock(mutex+MUTEX_PARSER);
    reply->add_to_object("count", new JsonValue(0));
    reply->add_to_object("result", new JsonValue(json_array));
    reply->add_to_object("error", new JsonValue("JSON access error"));
  }

  delete s;
}


bool exec_check() {
  return true;
}


bool app_wait(void) {
  if(!exec_check()) return true;

  FileAccess lock_file(cfg.path + "/indexer.lock");
  if(!lock_file.create()) {
    write_log(LOG_LEVEL_ERROR, "lock file create failed!", cfg.log_file);
    return false;
  }

  pid_t child_pid = fork();
  if(child_pid == -1) {
    write_log(LOG_LEVEL_ERROR, "index fork failed", cfg.log_file);
    return false;
  } else if(child_pid != 0) {  //parent
    return true;
  }

  // child
  set_child_signal();
  INSERT_REGULAR_INDEX_SET indexes;

  // remote source data
  /*
  while(indexer.load_source_documents(indexes)) {
    indexer.do_index(indexes, shm);
    indexes.clear();
    usleep(INDEX_WAIT_DURATION);   
  }

  // external source data
  while(indexer.parse_file(indexes, cfg.data_file)) {
    indexer.do_index(indexes);
    indexes.clear();
    FileAccess::remove(cfg.data_file);
    usleep(INDEX_WAIT_DURATION);   
  }
  */


  lock_file.remove();
  exit(0);
}





void app_exit(int sig) {
  write_log(LOG_LEVEL_INFO, "waiting other process...", cfg.log_file);
  if(!shm.lock_all()) {
    write_log(LOG_LEVEL_ERROR, "failed to wait other process...", cfg.log_file);
  }

  write_log(LOG_LEVEL_INFO, "release...", cfg.log_file);
  shm.release();

  write_log(LOG_LEVEL_INFO, "process stopped safety", cfg.log_file);

  exit(0);
}

void app_child_exit(int sig) {
  write_log(LOG_LEVEL_INFO, "terminate child process", cfg.log_file);
  exit(0);
}


void set_default_signal() {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = app_exit;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  act.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &act, NULL);
}

void set_child_signal() {
  struct sigaction act;
  memset(&act, 0, sizeof(act));
  act.sa_handler = app_child_exit;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGTERM, &act, NULL);

  act.sa_handler = SIG_IGN;
  sigaction(SIGHUP, &act, NULL);
}



/***************************************************
 *   main
 * *************************************************/
int main(int argc, char* const argv[]) {
  if(!get_options(argc, argv)) {
    std::cerr << "option error!!\n";
    std::cerr << "[usage]\n";
    std::cerr << "typhoon [-D data_dir] [-L log_file] [-P pid_file] [-p port] [-t]\n";
    exit(1);
  } 
  if(!cfg.directory_check()) {
    std::cerr << "directory check error!!\n";
    exit(1);
  } 

  // format
  if(cfg.data_file != "") {
    if(!format()) {
      write_log(LOG_LEVEL_ERROR, "data format failed", "");
      exit(1);
    }

    write_log(LOG_LEVEL_INFO, "data format completed", "");
    exit(0);
  }

  if(!cfg.load_conf()) {
    std::cerr << "configuration file error!!\n";
    exit(1);
  }

  shm.c1 = shm.c2 = shm.c3 = 0;
  shm.set_path(cfg.path);
  if(!shm.setup(cfg.page_size, cfg.block_size)) {
    std::cerr << "memory allocate error!!\n";
    exit(1);
  }

  // signal setting
  set_default_signal();
  FileAccess::empty_buffer_init();

  if(cfg.daemon) {
    write_log(LOG_LEVEL_INFO, "starting daemon...", "");
    int pid = daemonize();
    char pidstr[30];
    sprintf(pidstr, "PID is %d", pid);
    write_log(LOG_LEVEL_INFO, pidstr, cfg.log_file);

    if(cfg.pid_file != "") {
      std::ofstream pid_ofs(cfg.pid_file.c_str());
      if(!pid_ofs) {
        std::cout << "PID file open error: " << cfg.pid_file << "\n";
        write_log(LOG_LEVEL_ERROR, "PID file open failed: " + cfg.pid_file, cfg.log_file);
      }
      pid_ofs << pid;
      pid_ofs.close();
    }
  }

  // server mode
  try {
    Server s(app_request_handler, app_wait);
    s.start(cfg.port);
  } catch(AppException e) {
    write_log(LOG_LEVEL_ERROR, e.what(), cfg.log_file);
  }

  return 0;
}



//////////////////////////////////////////////////////////////////////
//  data format and initialize
//////////////////////////////////////////////////////////////////////
bool format() {
  std::string line;
  std::ifstream ifs(cfg.data_file.c_str());

  std::cout << "[setup and initializing]\n";
  std::string settings;
  while(getline(ifs, line)) {
    if(line == "--") break;
    settings = settings + line;
  }

  JsonValue* settings_val = JsonImport::json_import(settings.c_str());  
  if(!settings_val) {
    std::cerr << "[ERROR] invalid JSON format.\n";
    return false;
  }
  if(settings_val->get_value_type() != json_object) {
    std::cerr << "[ERROR] settings must be object type.\n";
    return false;
  }
  cfg.import_attrs_from_json(settings_val);
  cfg.dump();
  std::cout << "\n\n\n";

  JsonValue* memsize_val = settings_val->get_value_by_tag("memory_size");
  if(memsize_val) {
    if(memsize_val->get_value_type() == json_string) {
      std::string s = memsize_val->get_string_value();
      if(s == "minimum") cfg.block_size = MINIMUM_MEMORY_BLOCK;
      else if(s == "small") cfg.block_size = SMALL_MEMORY_BLOCK;
      else if(s == "medium") cfg.block_size = MEDIUM_MEMORY_BLOCK;
      else if(s == "large") cfg.block_size = LARGE_MEMORY_BLOCK;
      else if(s == "huge") cfg.block_size = HUGE_MEMORY_BLOCK;
      else {
        std::cerr << "[ERROR] memory_size must be specified by [minimum/small/medium/large/huge].\n";
        return false; 
      }
    }
    else if(memsize_val->get_value_type() == json_integer) {
      cfg.block_size = memsize_val->get_integer_value();
      if(cfg.block_size < MIN_MEMORY_BLOCK || cfg.block_size > MAX_MEMORY_BLOCK) {
        std::cerr << "[ERROR] memory_size is between 10 and 65535.\n";
        return false; 
      }
    }
    else {
      std::cerr << "[ERROR] memory_size must be integer or string.\n";
      return false; 
    }
  }
  shm.set_path(cfg.path);
  shm.init(cfg.page_size, cfg.block_size);
  
  Indexer* i = new Indexer(cfg.path, cfg.log_file, &cfg.attrs, &shm, &morph, &common_buf);
  i->data.init();
  delete i;

  clock_t total;
  clock_t total_c1;
  clock_t total_c2;
  clock_t total_c3;
  int query_count;


  std::cout << "[insert initial data and benchmark]\n";
  total = total_c1 = total_c2 = total_c3 = 0;
  shm.x1 = shm.x2 = shm.x3 = 0;
  query_count = 0;

  while(getline(ifs, line)) {
    if(line == "--") break;
    clock_t start = clock();
    shm.c1 = shm.c2 = shm.c3 = 0;

    std::string reply = app_request_handler(line.c_str(), NULL, INDEX_FLAG_CONT);
    if(shm.c1 != 0) total_c1 = total_c1 + shm.c1;
    if(shm.c2 != 0) total_c2 = total_c2 + shm.c2;
    if(shm.c3 != 0) total_c3 = total_c3 + shm.c3;
    total = total + clock() - start;
    query_count++;
  }

  if(query_count > 0) {
    app_request_handler(NULL, NULL, INDEX_FLAG_FIN);

    std::cout << "------------------------\n";
    std::cout << "total query: " << query_count << "\n";
    std::cout << "total time: " << (total*1.0)/CLOCKS_PER_SEC << "\n";
    if(total_c1 != 0) std::cout << "  bench1: " << (total_c1*1.0)/CLOCKS_PER_SEC  << "\n"; 
    if(total_c2 != 0) std::cout << "  bench2: " << (total_c2*1.0)/CLOCKS_PER_SEC  << "\n"; 
    if(total_c3 != 0) std::cout << "  bench3: " << (total_c3*1.0)/CLOCKS_PER_SEC  << "\n"; 
    std::cout << "total bench count 1: " << shm.x1 << "\n";
    std::cout << "total bench count 2: " << shm.x2 << "\n";
    std::cout << "\n\n\n";
  }

  std::cout << "[search test and benchmark]\n";
  total = total_c1 = total_c2 = total_c3 = 0;
  shm.x1 = shm.x2 = shm.x3 = 0;
  query_count = 0;

  while(getline(ifs, line)) {
    clock_t start = clock();
    shm.c1 = shm.c2 = shm.c3 = 0;

    std::string reply = app_request_handler(line.c_str(), NULL, INDEX_FLAG_FIN);
    std::cout << reply;
    if(shm.c1 != 0) total_c1 = total_c1 + shm.c1-start;
    if(shm.c2 != 0) total_c2 = total_c2 + shm.c2-start;
    if(shm.c3 != 0) total_c3 = total_c3 + shm.c3-start;
    total = total + clock() - start;
    query_count++;
  }

  if(query_count > 0) {
    std::cout << "------------------------\n";
    std::cout << "total_query: " << query_count << "\n";
    std::cout << "total time: " << (total*1.0)/CLOCKS_PER_SEC << "\n";
    if(total_c1 != 0) std::cout << "  bench1: " << (total_c1*1.0)/CLOCKS_PER_SEC  << "\n"; 
    if(total_c2 != 0) std::cout << "  bench2: " << (total_c2*1.0)/CLOCKS_PER_SEC  << "\n"; 
    if(total_c3 != 0) std::cout << "  bench3: " << (total_c3*1.0)/CLOCKS_PER_SEC  << "\n"; 
    std::cout << "total bench count 1: " << shm.x1 << "\n";
    std::cout << "total bench count 2: " << shm.x2 << "\n";
    std::cout << "\n\n\n";
  }

  cfg.save_conf();
  return true;
}


