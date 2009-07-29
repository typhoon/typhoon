/**********************************************************
 *  server.cc
 *    brief: Multiconnection server class with some options.
 *
 *  $Author: imamura $
 *  $Date:: 2009-03-31 18:55:26 +0900#$
 *
 *  Copyright (C) 2008 Drecom Co.,Ltd. All Rights Reserved.
 **********************************************************/
#include "server.h"
#include "exception.h"

using namespace std;


// static variables
std::string(*Server::app_func)(const char*, pthread_mutex_t*, int) = NULL;
bool(*Server::wait_func)(void) = NULL;
AllowedHostContainer Server::allowed_hosts;
pthread_mutex_t*   Server::mutex = NULL;



// constructor/destructor 
Server::Server( std::string(*fapp)(const char*, pthread_mutex_t*, int), bool(*fwait)(void) )
{
  end_flag = false;
  app_func = fapp;
  wait_func = fwait;

  // default: allow from private IP address.
  allowed_hosts.push_back( AllowedHost(0x0A000000, 8)  ); // 10.0.0.0/8
  allowed_hosts.push_back( AllowedHost(0xAC100000, 12) ); // 172.16.0.0/12
  allowed_hosts.push_back( AllowedHost(0xC0A80000, 16) ); // 192.168.0.0/16
  allowed_hosts.push_back( AllowedHost(0x7F000001, 32) ); // 127.0.0.1/32
}


Server::~Server()
{
  end();
}


//
void Server :: socket_set(struct sockaddr_in& src_addr, unsigned short port) {
  memset( &src_addr, 0, sizeof(src_addr) );
  src_addr.sin_port = htons(port);
  src_addr.sin_family = AF_INET;
  src_addr.sin_addr.s_addr = htonl(INADDR_ANY);
}


std::string Server :: socket_read(int read_socket) {
  // const char end[] = {0x0a};  //end code
  int idle_time = 0;

  fd_set fdset;
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  
  bool  multiline_mode = false; 
  char  buffer[BUFFER_SIZE+1];
  char  req[MAX_REQUEST_SIZE];
  int   req_pos = 0;

  std::string response = "";

  int buf_pos= 0;
  int numrcv = -1;
  while(1) {
    FD_ZERO(&fdset);
    FD_SET(read_socket, &fdset);

    bool completed = false;
    if(numrcv==-1 && select(read_socket+1, &fdset, NULL, NULL, &tv) == 1) { // buffer complete && exists recv data
      errno = 0;
      numrcv = recv(read_socket, buffer, BUFFER_SIZE, 0);
      if(numrcv == 0 || errno == EINTR) throw AppException(EX_APP_SERVER, "Connection closed");
      buf_pos = 0;
      idle_time = 0;
    }

    if(numrcv == -1) {
      usleep(CONNECT_DURATION);
      idle_time += CONNECT_DURATION;
      if(idle_time > TIMEOUT*100000) throw AppException(EX_APP_SERVER, "Connection timeout");
    }

    while(numrcv != -1) {
      if(buf_pos >= numrcv) {
        numrcv = -1;
        break;
      }

      if(buffer[buf_pos]=='\r' || buffer[buf_pos]=='\n' || buffer[buf_pos] == '\0') {
        req[req_pos++] = '\0';
        buf_pos++;
        completed = true;
        break;
      }

      req[req_pos++] = buffer[buf_pos++];
      if(req_pos > MAX_REQUEST_SIZE)  throw AppException(EX_APP_SERVER, "Too large request");
    }

    if(completed) {
      req_pos = 0;
      if(multiline_mode) {
        if(strcmp(req, "END") == 0) { // multiline end
          response = (*app_func)(NULL, mutex, INDEX_FLAG_FIN);
          break;
        } else {  // multiline continue
          (*app_func)(req, mutex, INDEX_FLAG_CONT);
        }
      } else {
        if(strcmp(req, "BEGIN") == 0) {  // multiline start
          multiline_mode = true;
        } else { 
          response = (*app_func)(req, mutex, INDEX_FLAG_FIN); // singleline
          break;
        }
      }
    }
  }
    
  return response;
}


void Server :: socket_write(int write_socket, std::string response) {
  send(write_socket, response.c_str(), response.length(), 0);
}



void Server::start(unsigned short port) 
{
  int src_socket;    
  int dst_socket;  
  struct sockaddr_in src_addr; 
  struct sockaddr_in dst_addr;  
  socklen_t dst_addr_length = sizeof(dst_addr); 

  socket_set(src_addr, port);

  src_socket = socket(PF_INET, SOCK_STREAM, 0);
  if (src_socket == -1)  throw AppException(EX_APP_SERVER, "Unable to create socket");
  int on = 1;
  setsockopt( src_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on) );

  if (bind( src_socket,(struct sockaddr *)&src_addr, sizeof(src_addr)) == -1) {
    switch (errno) {
    case EINVAL:
    case EADDRINUSE:
      throw AppException(EX_APP_SERVER, "Port already used");
    case EACCES:
      throw AppException(EX_APP_SERVER, "Port access violation");
    case EBADF:
      throw AppException(EX_APP_SERVER, "Invalid file descriptor");
    default:
      throw AppException(EX_APP_SERVER, "Failed to bind");
    }
  }

  if (listen( src_socket, SOMAXCONN ) == -1) {throw AppException(EX_APP_SERVER, "Failed to listen");}
  // fcntl( src_socket, F_SETFL, O_NONBLOCK );

  // initialize mutex 
  mutex = new pthread_mutex_t[MUTEX_COUNT]; 
  for(unsigned int i=0; i<MUTEX_COUNT; i++) pthread_mutex_init(&mutex[i], NULL);

  // initialize connection slot
  ServerThreadArg* args = new ServerThreadArg[MAX_CONNECTION];
  for(int i=0; i<MAX_CONNECTION; i++) {
    args[i].available = true, args[i].joinable = false;
  }

  while (!end_flag) {
    dst_socket = accept( src_socket,(struct sockaddr *)&dst_addr, &dst_addr_length);

    if(dst_socket == -1) {
      usleep(CONNECT_DURATION);
      continue;
    }

    bool complete = false;
    while(!complete) {
      for(int i=0; i<MAX_CONNECTION; i++) {
        if(args[i].joinable) {
          pthread_join(args[i].th, NULL);
          args[i].joinable  = false;
          args[i].available = true;
        }
      }

      if(dst_socket == -1) {
        usleep(CONNECT_DURATION);
        break;
      }

      for(int i=0; i<MAX_CONNECTION; i++) {
        if(args[i].available) {
          args[i].conn_sock = dst_socket;
          args[i].conn_addr = dst_addr;
          args[i].joinable = false;
          args[i].available = false;
          pthread_create(&args[i].th, NULL, thread_main, (void*)(&args[i]));
          complete = true;
          break;
        }
      }
    }
  }
  delete[] mutex;
  delete[] args;

  close(src_socket);
}


bool Server::is_allowed_host(const sockaddr_in &addr)
{
  bool allowed = false;
  long host = ntohl( addr.sin_addr.s_addr );

  for (AllowedHostContainer::iterator it = allowed_hosts.begin(); it != allowed_hosts.end(); it++) {
    // Creating bitmask
    unsigned long mask = 0xFFFFFFFF;
    if (it->second > 0) {
      mask = (1 << ((unsigned long)sizeof(unsigned long) * 8 - it->second)) - 1;
    }
              
    if ((it->first | mask) == (host | mask)) {
      allowed = true;
      break;
    }
  }        
      
  return allowed;
}


void* Server::thread_main(void* p) {
  ServerThreadArg* arg = (ServerThreadArg*)p;
  std::string response = "";

  try {
    if(!is_allowed_host(arg->conn_addr)) throw AppException(EX_APP_SERVER, "connection denied");
    response = socket_read(arg->conn_sock);
    socket_write(arg->conn_sock, response);
  } catch(AppException e) {
    std::cout << e.what() << "\n";
    response = e.what();
  }

  close(arg->conn_sock);
  arg->joinable = true;
  pthread_exit(0);
}
