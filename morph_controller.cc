/*****************************************************************
 *  morph_controller.cc
 *    brief: Language parser on ja_JP.UTF-8
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-18 22:17:52 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#include <iostream>

#include "charset.h"
#include "morph_controller.h"


#ifdef CHARSET_JA_JP_UTF8

#include <mecab.h>

MorphController :: MorphController () {
  const char* opt_list[3] = {"mecab", "-O", "chasen"};
  tagger = mecab_new(3, (char**)opt_list);
}

MorphController :: ~MorphController () {
  if(tagger)  mecab_destroy((mecab_t*)tagger);
}

unsigned int MorphController::get_morph_data(const char* input, MORPH_DATA_SET& morphs) {
  const mecab_node_t* node = mecab_sparse_tonode((mecab_t*)tagger, input);
  if (!node) return 0;

  // defined along IPA 
  const char type[24][2][100] = {
    {"連体詞", ""},
    {"接頭詞", ""},
    {"名詞", "一般"},
    {"名詞", "サ変接続"},
    {"名詞", "形容動詞語幹"},
    {"名詞", "ナイ形容詞語幹"},
    {"名詞", "数"},
    {"名詞", "固有名詞"},
    {"名詞", "接尾"},
    {"名詞", "代名詞"},
    {"名詞", "副詞可能"},
    {"名詞", ""},
    {"動詞", "自立"},
    {"動詞", ""},
    {"形容詞", "自立"},
    {"形容詞", ""},
    {"副詞", ""},
    {"接続詞", ""},
    {"助詞", ""},
    {"助動詞", ""},
    {"感動詞", ""},
    {"記号", ""},
    {"未知語", ""},
    {"", ""}
  };

  // index, ignore, before_terminate, after_terminate
  MorphemeData ptn[24] = {
    {true, false, false, false},  //連体詞
    {true, false, true, false},  //接頭詞
    {true, false, false, false},  //名詞、一般
    {true, false, false, false},  //名詞、サ変接続
    {true, false, false, false},  //名詞、形容動詞語幹
    {true, false, false, false},  //名詞、ナイ形容詞語幹
    {true, false, false, false},  //名詞、数
    {true, false, false, false},  //名詞、固有名詞
    {true, false, false, false},  //名詞、接尾
    {true, false, false, false},  //名詞、代名詞
    {true, false, false, false},  //名詞、副詞可能
    {false, false, false, false}, //名詞、その他
    {true, false, false, false},  //動詞、自立
    {false, false, false, false}, //動詞、その他
    {true, false, false, false},  //形容詞、自立
    {false, false, false, false}, //形容詞、その他
    {true, false, false, false},  //副詞
    {true, false, true, true}, //接続詞
    {false, true, false, false}, //助詞
    {false, true, false, false}, //助動詞
    {true, false, false, false},  //感動詞
    {false, true, true, true},    //記号
    {true, false, false, false},  //未知語
    {false, true, true, true}     //その他
  };
  MorphemeData invalid_ptn = {false, true, true, true};


  int prev_pos = 0;
  for (; node; node = node->next) {
    if (node->stat != MECAB_BOS_NODE && node->stat != MECAB_EOS_NODE) {

      const char* f1 = node->feature;
      int   f1_len = 0;
      while(*(f1+f1_len) != ',' && *(f1+f1_len) != '\0') f1_len++;
      const char* f2 = f1+f1_len;
      int   f2_len = 0;
      while(*f2 == ',') f2++;
      while(*(f2+f2_len) != ',' && *(f2+f2_len) != '\0') f2_len++; 

      MorphemeData md = {false, true, true, true, false, false, node->surface, node->length};
      word_check(md);
      if(md.valid) {
        for(unsigned int i=0; i<24; i++) {
          if((strcmp(type[i][0], "") == 0 || strncmp(f1, type[i][0], f1_len) == 0) && 
             (strcmp(type[i][1], "") == 0 || strncmp(f2, type[i][1], f2_len) == 0)) {
              md.index = ptn[i].index, md.ignore = ptn[i].ignore;
              md.before_terminate = ptn[i].before_terminate, md.after_terminate = ptn[i].after_terminate;
              break;
          }
        } 
      } else {
        md.index = invalid_ptn.index, md.ignore = invalid_ptn.ignore;
        md.before_terminate = invalid_ptn.before_terminate, md.after_terminate = invalid_ptn.after_terminate;
      }
 
      // if word is splitted by space set before_terminate
      if(prev_pos != (int)(node->surface - input)) md.before_terminate = true;

      morphs.push_back(md);
    }

    prev_pos = (int)(node->surface - input + node->length);
  }

  return morphs.size();
}


unsigned int MorphController::get_search_phrases(const char* input, WORD_SET& result, unsigned int maxlen) {
  MORPH_DATA_SET m;
  get_morph_data(input, m); 

  for(unsigned int i=0; i<m.size(); i++) {
    if(m[i].ignore) continue;
  
    if(m[i].before_terminate) result.push_back("");
    result.push_back(std::string(m[i].surface, m[i].length));
  }

/*
  WORD_SET phrase_set;
  unsigned int i=0;
  unsigned int rpos = 0;
  std::string phrase = "";
  while(i<m.size()) {
    if(m[i].index) {
      phrase = std::string(m[i].surface, m[i].length);
      unsigned int j;
      for(j=1; j<maxlen; j++) {
        if(i+j >= m.size()) break;
        if(m[i+j].before_terminate) break;
        if(!m[i+j].ignore) phrase = phrase + std::string(m[i+j].surface, m[i+j].length);
        if(m[i+j].after_terminate) break;
      }
      if(phrase != "" && rpos < i+j) {
        result.push_back(std::string(phrase, 0, MAX_PHRASE_LENGTH));
        rpos = i+j;
      }
    }
    i++;
  }
*/

  return result.size();
}


bool MorphController::word_check(MorphemeData& md) {
  unsigned int i=0;
  md.valid = false;
  md.multibyte = false;

  while(i < md.length) {
    CharInfo info = CharsetController::get_char_info(md.surface, md.length-i);
    if(info.err != ERR_NOCHARERR) {
      md.valid = false;
      md.multibyte = false;
      return false;
    }
    else if(info.byte > 1) {
      md.valid = true;
      md.multibyte = true;
    }
    else {
      char c= *(md.surface+i); 
      if(isalnum(c) != 0) md.valid = true;
    } 
    i+= info.byte;
  }

  return true;
}

bool MorphController::test() {
  std::string str;
  
  MORPH_DATA_SET morphs;

  str = "the世界一社内ブログを売る男・山本直人デビュー";
  get_morph_data(str.c_str(), morphs); 

  std::cout << "---parse result---\n";
  for(unsigned int i=0; i<morphs.size(); i++) {
    std::string word = std::string(morphs[i].surface, morphs[i].length);
    std::cout << word;
    std::cout << "\t";
    std::cout << (morphs[i].index ? "index" : "noindex");
    std::cout << "\t";
    std::cout << (morphs[i].ignore ? "ignore" : "use");
    std::cout << "\t";
    std::cout << (morphs[i].after_terminate ? "after" : (morphs[i].before_terminate ? "before" : "none"));
    std::cout << "\t";
    std::cout << (morphs[i].multibyte ? "multibyte" : "ascii");
    std::cout << "\n";
  }
/*
  INSERT_PHRASE_SET w;
  w.clear();
  get_index_phrases(str.c_str(), w, 3);
  std::cout << "---index phrases---\n";
 
  WORD_SET w2; 
  w2.clear(); 
  get_search_phrases(str.c_str(), w2, 3);
  std::cout << "---search phrases---\n";


  morphs.clear();
  str = "This is a pen. 英語のフレーズが入っても解析できるかどうかテスト";
  get_morph_data(str.c_str(), morphs); 
  std::cout << "---parse info---\n";
  for(unsigned int i=0; i<morphs.size(); i++) {
    std::string word = std::string(morphs[i].surface, morphs[i].length);
    std::cout << word;
    std::cout << "\t";
    std::cout << (morphs[i].index ? "index" : "noindex");
    std::cout << "\t";
    std::cout << (morphs[i].ignore ? "ignore" : "use");
    std::cout << "\t";
    std::cout << (morphs[i].after_terminate ? "after" : (morphs[i].before_terminate ? "before" : "none"));
    std::cout << "\t";
    std::cout << (morphs[i].multibyte ? "multibyte" : "ascii");
    std::cout << "\n";
  }
  
  w.clear();
  get_index_phrases(str.c_str(), w, 3);
  std::cout << "---index phrases---\n";
  
  w2.clear(); 
  get_search_phrases(str.c_str(), w2, 3);
  std::cout << "---search phrases---\n";
*/
  return true;
}

#endif


