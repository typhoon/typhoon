/*****************************************************************
 *  checker.cc
 *    brief:
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-08 19:53:02 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/

#include "common.h"
#include "checker.h"

CheckerConfig  cfg;
DataController data;
SharedMemoryAccess  shm(0, 0);
MorphController     morph;
Buffer common_buf;

bool get_options(int argc, char* const argv[]) {
  // initialize
  char* path_buf = new char[MAX_PATH_LENGTH+1];
  getcwd(path_buf, MAX_PATH_LENGTH);
  cfg.path = "/tmp/typhoontest";
  delete[] path_buf;

  cfg.test_mode = "";
  cfg.dump_mode = "";

  // option setting
  char optchar;
  opterr = 0;
  while((optchar=getopt(argc, argv, "D:x:X:t:")) != -1) {
    if(optchar == 'D')      cfg.path = std::string(optarg);
    else if(optchar == 'x') {cfg.dump_id = atoi(optarg);cfg.dump_mode = "id";}
    else if(optchar == 'X') cfg.dump_mode = std::string(optarg);
    else if(optchar == 't') {cfg.test_mode = std::string(optarg);}
    else {
      return false;      
    }
  }

  return true;
}


void test() {
  WORD_SET modules = split(cfg.test_mode, ",");

  for(unsigned int i=0; i<modules.size(); i++) {
    std::string work_path = cfg.path;
    FileAccess::create_directory(work_path);

    if(modules[i] == "conf" || modules[i] == "all") {
      std::cout << ">>>>configuration test...\n";
      AppConfig c;
      if(!c.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }


    if(modules[i] == "morph" || modules[i] == "all") {
      std::cout << ">>>>checking text parser module... \n";
      MorphController m;
      if(!m.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "buffer" || modules[i] == "all") {
      std::cout << ">>>>checking buffer module...\n";
      Buffer b;
      if(!b.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "shm" || modules[i] == "all") {
      std::cout << ">>>>checking shared memory module...\n";
      SharedMemoryAccess s(0, 0); 
      s.set_path(work_path);
      if(!s.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }


    if(modules[i] == "file" || modules[i] == "all") {
      std::cout << ">>>>checking file access module... \n";
      FileAccess f;
      SharedMemoryAccess s(getpagesize()*4, 10); 
      s.set_path(work_path);
      f.set_file_name(work_path, DATA_TYPE_REVERSE_INDEX, "dat");
      f.set_shared_memory(&s);
      if(!f.test()) {
        f.remove_with_suffix();
        std::cout << "error\n";
        exit(1);
      }
      f.remove_with_suffix();
    }

    DocumentDataController doc_data;
    DocumentController doc; 
    PhraseDataController ph_data;
    PhraseController ph;
    RegularIndexController reg;
    ReverseIndexController rev;

    shm.set_path(work_path);
    if(modules[i] == "ddata" || modules[i] == "all") {
      std::cout << ">>>>checking document data module...\n";
      shm.init(getpagesize()*4, 100);
      doc_data.init(work_path, &shm);
      if(!doc_data.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "daddr" || modules[i] == "all") {
      std::cout << ">>>>checking document index module...\n";
      shm.init(getpagesize()*4, 100);
      doc_data.init(work_path, &shm);
      doc.init(work_path, &shm);
      doc.set_document_data(&doc_data);
      if(!doc.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }
   
    if(modules[i] == "pdata" || modules[i] == "all") { 
      std::cout << ">>>>checking phrase data module...\n";
      shm.init(getpagesize()*4, 100);
      ph_data.init(work_path, &shm);
      if(!ph_data.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "paddr" || modules[i] == "all") {
      std::cout << ">>>>checking phrase index module...\n";
      shm.init(getpagesize()*4, 100);
      ph.init(work_path, &shm);
      ph_data.init(work_path, &shm);
      ph.set_phrase_data(&ph_data);
      if(!ph.test())  {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "regindex" || modules[i] == "all") {
      std::cout << ">>>>checking regular index module...\n";
      shm.init(getpagesize()*4, 100);
      doc_data.init(work_path, &shm);
      doc.init(work_path, &shm);
      doc.set_document_data(&doc_data);
      ph.init(work_path, &shm);
      ph_data.init(work_path, &shm);
      ph.set_phrase_data(&ph_data);

      reg.init(work_path, &shm);
      reg.set_document_and_phrase(&doc, &ph);
      if(!reg.test()) {
        std::cout << "error\n";
        exit(1);
      }
    }

    if(modules[i] == "revindex" || modules[i] == "all") {
      std::cout << ">>>>checking reverse index module...\n";
      shm.init(getpagesize()*4, 100);
      doc_data.init(work_path, &shm);
      doc.init(work_path, &shm);
      doc.set_document_data(&doc_data);
      ph.init(work_path, &shm);
      ph_data.init(work_path, &shm);
      ph.set_phrase_data(&ph_data);

      rev.init(work_path, &shm);
      rev.set_document_and_phrase(&doc, &ph);
      if(!rev.test()) {
        std::cout << "test failed\n";
        exit(1);
      }
    }

    if(modules[i] == "indexer" || modules[i] == "all") {
      std::cout << ">>>>checking indexer application...\n";
      shm.init(getpagesize()*4, 100);
      data.init(work_path, &shm);
      cfg.import_attrs_from_string("{\"columns\":{\"id\":\"pkey\", \"content\":\"fulltext\", \"title\":\"fulltext\", \"age\":\"integer\", \"rank\":\"integer,noindex\"}, \"sortkeys\":[\"rank,desc\"]}");

      Indexer indexer(work_path, "", &cfg.attrs, &shm, &morph, &common_buf);
      if(!indexer.test()) {
        std::cout << "failed\n";
        exit(1);
      }
    } 

    if(modules[i] == "searcher" || modules[i] == "all") {
      std::cout << ">>>>cheking searcher application...\n";
      shm.init(getpagesize()*4, 100);
      cfg.import_attrs_from_string("{\"columns\":{\"id\":\"pkey\", \"content\":\"fulltext\", \"title\":\"string\", \"age\":\"integer\", \"rank\":\"integer,noindex\"}, \"sortkeys\":[\"rank,desc\"]}"); 

      // setup testdata
      Indexer indexer(work_path, "", &cfg.attrs, &shm, &morph, &common_buf);
      Searcher searcher(work_path, "", &cfg.attrs, &shm);

      std::cout << ">>>> test for empty data\n";
      if(!searcher.test()) {
        std::cout << "failed\n";
        exit(1);
      } 
    }

    std::cout << ">>>>all test completed!!\n";
  }

}

void dump(void) {
  DocumentAddr addr = data.document.find_addr(cfg.dump_id);
}


void dumpall() {
  if(!data.setup(cfg.path, &shm)) {
    write_log(LOG_LEVEL_ERROR, "content setup failed");
    return;
  }
  
}




/***************************************************
 *   main
 * *************************************************/
int main(int argc, char* const argv[]) {
  if(!get_options(argc, argv)) {
    std::cerr << "option error!!\n";
    std::cerr << "[usage]\n";
    std::cerr << "checker [-x dump_id] [-X dump_mode] [-t test_mode]\n";
    exit(1);
  }

  if(cfg.test_mode != "") {
    test();
    exit(0);    
  }

  if(!cfg.directory_check()) {
    std::cerr << "directory check error!!\n";
    exit(1);
  }

  if(!cfg.load_conf()) {
    std::cerr << "configuration file error!!\n";
    exit(1);
  }

  shm.set_path(cfg.path);
  if(!shm.setup(cfg.page_size, cfg.block_size)) {
    std::cerr << "memory allocation error!!\n";
    exit(1);
  }

  if(!data.setup(cfg.path, &shm)) {
    write_log(LOG_LEVEL_ERROR, "content setup failed");
    exit(1);
  }


  if(cfg.dump_mode == "id") {
    dump();
    return 0;
  } else if(cfg.dump_mode == "") {
    dumpall();
    return 0;
  } else {
    try {
      data.dump(cfg.dump_mode);
    } catch(AppException e) {
      write_log(LOG_LEVEL_ERROR, e.what());
    }
    return 0;
  } 

  return 0;
}




