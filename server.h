/*****************************************************************
 *  server.h
 *    brief:
 *
 *  $Author: imamura_shoichi $
 *  $Date:: 2009-04-14 09:38:07 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 ****************************************************************/


#ifndef __SERVER_H__
#define __SERVER_H__

#include <string>
#include <vector>
#include <iostream>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "common.h"

#define MUTEX_COUNT 10
#define MUTEX_CONN_COUNTER    0
#define MUTEX_SEARCHER        1
#define MUTEX_INDEXER         2
#define MUTEX_PARSER          3
#define MUTEX_INDEXER_PROC1   4
#define MUTEX_INDEXER_PROC2   5
#define MUTEX_INDEXER_PROC3   6
#define MUTEX_INDEXER_PROC4   7


typedef std::pair<unsigned long, char> AllowedHost;
typedef std::vector<AllowedHost> AllowedHostContainer;


struct ServerThreadArg {
  pthread_t           th;
  int                 conn_sock;
  struct sockaddr_in  conn_addr;
  bool                available;
  bool                joinable;

  pthread_mutex_t*    mutex;
};


class Server {
public:
    Server( std::string(*)(const char*, pthread_mutex_t*, int), bool(*)(void) );
    virtual ~Server();

    void start(unsigned short port);
    static void* thread_main(void*);

private:
    const static int MAX_CONNECTION = 100;
    const static int MAX_REQUEST_SIZE = 1000000;  // 1MB
    const static int BUFFER_SIZE = 4096;
    const static int TIMEOUT     = 30;
    const static int CONNECT_DURATION = 200;

    static AllowedHostContainer allowed_hosts;
    static pthread_mutex_t*   mutex;

    static std::string(*app_func)(const char*, pthread_mutex_t*, int);  // main application
    static bool(*wait_func)(void);  // connection wait application
    static std::string socket_read(int);
    static void socket_write(int, std::string);
    static bool is_allowed_host(const sockaddr_in&);
   

    // not static 
    bool end_flag;

    Server();

    void socket_set(sockaddr_in&, unsigned short int);
    inline void end(void) {end_flag = true;}
};


#endif

