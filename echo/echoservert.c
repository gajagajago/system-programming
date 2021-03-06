//------------------------------------------------------------------------------
/// @file  echoservert.c
/// @brief echo server (threaded version)
///
/// @author Bernhard Egger <bernhard@csap.snu.ac.kr>
/// @section changelog Change Log
/// - 2016/10/14 Bernhard Egger created
/// - 2017/11/24 Bernhard Egger added put/get_line functions
/// - 2017/12/05 Bernhard Egger cleanup & bugfixes
///
/// @section license_section License
/// Copyright (c) 2016-2017, Computer Systems and Platforms Laboratory, SNU
/// All rights reserved.
///
/// Redistribution and use in source and binary forms,  with or without modifi-
/// cation, are permitted provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice,
///   this list of conditions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice,
///   this list of conditions and the following disclaimer in the documentation
///   and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,  BUT NOT LIMITED TO,  THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY  AND FITNESS FOR A PARTICULAR PURPOSE
/// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER  OR CONTRIBUTORS BE
/// LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSE-
/// QUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF  SUBSTITUTE
/// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
/// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT
/// LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY
/// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//------------------------------------------------------------------------------

// standard headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

// for networking
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

// for pthreads
#include <pthread.h>

#include "common.h"
#include "net.h"

static char stx_str[] = "server [<port>]";

static int listen_fd = -1;            ///< active socket, closed in atexit()


/// @brief opens a listening socket on the specified port
///
/// Returns the file descriptor of the listening socket (>=0),
/// or aborts (does not return) on failure.
///
/// @param port port to bind to (host order)
/// @retval int file descriptor of listening socket
int open_port(uint16_t port)
{
  struct addrinfo *ai, *ai_it;
  int fd = -1, vtrue = 1;

  printf("Opening port %d...\n", port);

  //
  // get list of potential sockets
  //
  ai = getsocklist(NULL, port, AF_UNSPEC, SOCK_STREAM, 1, NULL);

  //
  // iterate through potential addressinfo structs and try one by one. Break out on first
  // that works.
  //
  ai_it = ai;
  while (ai_it != NULL) {
    printf("  trying "); dump_sockaddr(ai_it->ai_addr); printf("..."); fflush(stdout);

    fd = socket(ai_it->ai_family, ai_it->ai_socktype, ai_it->ai_protocol);
    if (fd != -1) {
      // allow immediate reuse of the address by bind() by setting SO_REUSEADDR=1
      if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&vtrue, sizeof(int)) == 0) &&
          (bind(fd, ai_it->ai_addr, ai_it->ai_addrlen) == 0) &&
          (listen(fd, 32) == 0)) break; // success, break out of loop
      close(fd);
    }
    printf("failed.\n");
    ai_it = ai_it->ai_next;
  }


  // ai_it == NULL -> binding/listening failed, abort
  if (ai_it == NULL) error("Cannot bind to port.");

  // free address info struct
  freeaddrinfo(ai);

  printf("success.\n");
  return fd;
}

void upper_case(char *s)
{
  while (*s) {
    *s = toupper(*s);
    s++;
  }
}

/// @brief receive data, uppercase it, then send it back to client. Exits on
///        stream error.
/// @param connfd communication socket (closed automatically when process ends)
void run_instance(int connfd) 
{
  pthread_t tid;
  int res;
  char *msg;
  size_t msg_len;

  tid = pthread_self();
  msg_len = 256;
  msg = (char*)malloc(msg_len);

  while (1) {
    printf("[EchoServer:receive %5lu] ", tid); fflush(stdout);

    // read a line from client
    res = get_line(connfd, &msg, &msg_len);
    if (res <= 0) break;

    printf("%s", msg); fflush(stdout);

    // convert to upper case
    upper_case(msg);

    printf("[EchoServer:send    %5lu] %s", tid, msg); 

    // send line to client
    res = put_line(connfd, msg, msg_len);
    if (res < 0) printf("Error: cannot send data to client (%d).\n", res);
  }

  printf("Connection closed by peer\n");

  free(msg);
  close(connfd);
}

/// @brief main thread routine
/// @param vargp pointer to socket fd
void* run_thread(void *vargp)
{
  int client_fd = *((int*)vargp);

  // detach ("self-reaping") and free pointer to connfd
  pthread_detach(pthread_self());
  free(vargp);

  // run receiver-sender loop
  run_instance(client_fd);

  return NULL;
}

/// @brief main server routine accepting new connections. Forks a new
///        instance for every client and calls run_instance() on it.
/// @param listen_fd listening socket
void run_server(int listen_fd)
{
  while (1) {
    int *client_fdp = malloc(sizeof(int));
    struct sockaddr client;
    socklen_t clientlen = sizeof(client);

    *client_fdp = accept(listen_fd, &client, &clientlen);

    if (*client_fdp > 0) {
      pthread_t tid;
      printf("  connection from "); dump_sockaddr(&client); printf("\n");

      pthread_create(&tid, NULL, run_thread, client_fdp);
    } else {
      // print error message and abort on any error
      perror("accept");
      break;
    }
  }
}

/// @brief close main connection (atexit() handler)
void close_connection(void)
{
  if (listen_fd != -1) {
    close(listen_fd);
    listen_fd = -1;
  }
}

/// @brief program entry point
int main(int argc, char *argv[])
{
  uint16_t port = 12345;          // default port

  // optional argument: port
  if (argc >= 2) {
    char *epos;
    int n = strtol(argv[1], &epos, 0);
    if (*epos != '\0') syntax("Invalid port number.", stx_str);
    if ((n < 0) || (n > 0xffff)) syntax("Port must be in range 0-65535.", stx_str);
    port = (uint16_t)n;
  }

  // open port on localhost
  listen_fd = open_port(port);
  atexit(close_connection);

  // run the server
  run_server(listen_fd);

  // that's all, folks
  return EXIT_SUCCESS;
}
