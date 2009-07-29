/******************************************************************
 * data_controller.cc
 *  $Author: imamura $
 *  $Date:: 2009-04-07 13:03:51 +0900#$ 
 *
 * Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 *******************************************************************/
#include "data_controller.h"

DataController::DataController() {

}


DataController::~DataController() {

}


bool DataController::init() {
  std::cout << "phrase initializing...\n";  
  phrase.init();
  std::cout << "phrase data initializing...\n";  
  phrase_data.init();
  std::cout << "document initializing...\n";  
  document.init();
  std::cout << "document data initializing...\n";
  document_data.init();
  std::cout << "regular index initializing...\n";  
  regular_index.init();
  std::cout << "reverse_index initializing...\n";  
  reverse_index.init();

  set_link();
  return true;
}

bool DataController::init
(std::string path, SharedMemoryAccess* shm) {
  std::cout << "phrase initializing...\n";  
  phrase.init(path, shm);
  std::cout << "phrase data initializing...\n";  
  phrase_data.init(path, shm);
  std::cout << "document initializing...\n";  
  document.init(path, shm);
  std::cout << "document data initializing...\n";
  document_data.init(path, shm);
  std::cout << "reverse_index initializing...\n";  
  reverse_index.init(path, shm);
  std::cout << "regular_index initializing...\n";  
  regular_index.init(path, shm);

  set_link();
  return true;
}

bool DataController::save() {
  bool result = true;

  if(!phrase.save()) {
    std::cout << "phrase save failed\n";
    result = false;
  }


  if(!phrase_data.save()) {
    std::cout << "phrase data save failed\n";
    result = false;
  }

  if(!document.save()) {
    std::cout << "document save failed\n";
    result = false;
  }
  if(!document_data.save()) {
    std::cout << "document data save failed\n";
    result = false;
  }

  if(!regular_index.save()) {
    std::cout << "regular index save failed\n";
    result = false;
  }
  if(!reverse_index.save()) {
    std::cout << "reverse_index save failed\n";
    result = false;
  }

  return result;
}

bool DataController::setup(std::string path, SharedMemoryAccess* shm) {
  bool result = true;

  set_link(); 

  if(!phrase.setup(path, shm)) {
    std::cout << "phrase setup failed\n";
    result = false;
  }
  if(!phrase_data.setup(path, shm)) {
    std::cout << "phrase data setup failed\n";
    result = false;
  }
  if(!document.setup(path, shm)) {
    std::cout << "document data setup failed\n";
    result = false;
  }
  if(!document_data.setup(path, shm)) {
    std::cout << "document data setup failed\n";
    result = false;
  }
  if(!regular_index.setup(path, shm)) {
    std::cout << "regular index data setup failed\n";
    result = false;
  }
  if(!reverse_index.setup(path, shm)) {
    std::cout << "reverse_index data setup failed\n";
    result = false;
  }

  return result;
}

bool DataController::reset() {
  reverse_index.reset();
  phrase.reset();
  phrase_data.reset();
  document.reset();
  document_data.reset();
  regular_index.reset();
  

  return true;
}


bool DataController::finish() {
  reverse_index.finish();
  phrase.finish();
  phrase_data.finish();
  document.finish();
  document_data.finish();
  regular_index.finish();

  return true;
}


void DataController::set_link() {
  document.set_document_data(&document_data);
  phrase.set_phrase_data(&phrase_data);
  reverse_index.set_document_and_phrase(&document, &phrase);
  regular_index.set_document_and_phrase(&document, &phrase);
}







/////////////////////////////////////////////////////////
//  debug
/////////////////////////////////////////////////////////
bool DataController::set_sample(unsigned char attr_header) {
  Buffer buf;
  INSERT_PHRASE_SET p_set;
  INSERT_DOCUMENT_SET d_set;
  unsigned int d_cnt = 3000;
  unsigned int p_cnt = d_cnt * 100;

  std::cout << "document sample...\n"; 
  for(unsigned int i=0; i<d_cnt; i++) {
    InsertDocument d = {{i, {0xFFFFFFFF ^ (i%10), 0, 0, 0}}, {0, NULL_DOCUMENT}};
    d_set.push_back(d);
    
  }
  document.insert(d_set);
 
  std::cout << "phrase sample...\n"; 
  for(unsigned int i=0; i<p_cnt; i++) {
    char phrase[10];
    sprintf(phrase, "p%06d", i);
    char* ptr = buf.allocate(strlen(phrase)+2);
    ptr[0] = attr_header;
    strcpy(ptr+1, phrase);

    InsertPhrase p = {0, 0, {ptr}, {0, NULL_PHRASE}};
    p_set.push_back(p);
  }
  phrase.insert(p_set);


  std::cout << "index sample...\n"; 
  for(unsigned int i=0; i<d_set.size(); i++) {
    INSERT_REGULAR_INDEX_SET regidx_set;
    INSERT_REVERSE_INDEX_SET revidx_set;

    InsertRegularIndex reg;
    reg.doc = d_set[i];
    reg.phrases.push_back(p_set[i%10]);
    reg.phrases.push_back(p_set[i%7+10]);
    reg.phrases.push_back(p_set[i%15+20]);
    for(int j=70; j<90; j++) {
      reg.phrases.push_back(p_set[i*100+j]);
    }
    reg.phrases.push_back(p_set[99]);

    regidx_set.push_back(reg);
   
    for(unsigned int k=0; k<reg.phrases.size(); k++) {
      InsertReverseIndex rev;
      rev.doc = reg.doc;
      rev.phrase = reg.phrases[k];
      rev.delete_flag = false;
      revidx_set.push_back(rev);
    }

    sort(revidx_set.begin(), revidx_set.end(), InsertReverseIndexComp());
    regular_index.insert(regidx_set);
    reverse_index.insert(revidx_set);
  }

  buf.clear();
  return true;
}


void DataController::dump(std::string mode) {
  if(mode == "paddr") {
    phrase.dump();
  }
  else if(mode == "pdata") {
    phrase_data.dump();
  }
  else if(mode == "daddr") {
    document.dump();
  }
  else if(mode == "ddata") {
    document_data.dump();
  }
  else if(mode == "regindex") {
    regular_index.dump();
  }
  else if(mode == "revindex") {
    reverse_index.dump();
  }
}
