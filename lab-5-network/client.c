//--------------------------------------------------------------------------------------------------
// System Programming                       Network Lab                                  Fall 2021
//
/// @file
/// @brief Client-side implementation of Network Lab
///
/// @author <your name>
/// @studid <your student id>
///
/// @section changelog Change Log
/// 2020/11/18 Hyunik Kim created
/// 2021/11/23 Jaume Mateu Cuadrat cleanup, add milestones
///
/// @section license_section License
/// Copyright (c) 2020-2021, Computer Systems and Platforms Laboratory, SNU
/// All rights reserved.
///
/// Redistribution and use in source and binary forms, with or without modification, are permitted
/// provided that the following conditions are met:
///
/// - Redistributions of source code must retain the above copyright notice, this list of condi-
///   tions and the following disclaimer.
/// - Redistributions in binary form must reproduce the above copyright notice, this list of condi-
///   tions and the following disclaimer in the documentation and/or other materials provided with
///   the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
/// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF MERCHANTABILITY
/// AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
/// CONTRIBUTORS  BE LIABLE FOR ANY DIRECT,  INDIRECT, INCIDENTAL, SPECIAL,  EXEMPLARY,  OR CONSE-
/// QUENTIAL DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
/// LOSS OF USE, DATA,  OR PROFITS; OR BUSINESS INTERRUPTION)  HOWEVER CAUSED AND ON ANY THEORY OF
/// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
/// DAMAGE.
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "net.h"
#include "burger.h"


/// @brief client task for connection thread
void *thread_task(void *data)
{
  struct addrinfo *ai, *ai_it;
  size_t read, sent, buflen;
  int serverfd = -1, choice, ret;
  char *buffer;
  pthread_t tid;

  tid = pthread_self();

  buffer = (char *)malloc(BUF_SIZE);
  buflen = BUF_SIZE;

  // Connect to McDonald's server
  // Get the socket list
  // TODO
  if((ai = getsocklist(IP, PORT, AF_UNSPEC, SOCK_STREAM, 0, NULL)) == NULL)
    perror("Can't get socket list\n");
  
  // Create the sockets and connect
  // TODO
  ai_it = ai;
 
  while(ai_it != NULL) {
    serverfd = socket(ai_it->ai_family, ai_it->ai_socktype, ai_it->ai_protocol);

    if(serverfd != -1) {
      if(connect(serverfd, ai_it->ai_addr, ai_it->ai_addrlen) == 0)
        break;  // success
      close(serverfd);  // failed to connect
    }
    ai_it = ai_it->ai_next;
  }

  // Read welcome message from the server
  read = get_line(serverfd, &buffer, &buflen);
  if (read <= 0) {
    printf("Cannot read data from server\n");
    goto err;
  }

  printf("[Thread %lu] From server: %s", tid, buffer);

  // Randomly choose which burger to order
  choice = rand() % BURGER_TYPE_MAX;
  ret = strlen(burger_names[choice]);
  printf("[Thread %lu] To server: Can I have a %s burger?\n", tid, burger_names[choice]);

  memset(buffer, 0, BUF_SIZE);
  strncpy(buffer, burger_names[choice], ret);
  buffer[ret] = '\n';

  // Send burger name to the server
  sent = put_line(serverfd, buffer, strlen(buffer));
  if (sent < 0) {
    printf("Error: cannot send data to server\n");
    goto err;
  }

  // Get final message from the server
  memset(buffer, 0, BUF_SIZE);
  read = get_line(serverfd, &buffer, &buflen);
  if (read <= 0) {
    printf("Cannot read data from server\n");
    goto err;
  }

  printf("[Thread %lu] From server: %s", tid, buffer);

err:
  close(serverfd);
  pthread_exit(NULL);
}

/// @brief program entry point
int main(int argc, char const *argv[])
{
  int i;
  int num_threads;

  if (argc != 2) {
    printf("usage ./client <num_threads>\n");
    return 0;
  }

  // Create the threads equal to the num of threads passed in the argument
  // TODO
  num_threads = atoi(argv[1]);
  pthread_t tid[num_threads];

  for(i=0; i<num_threads; i++) {
    if(pthread_create(&tid[i], NULL, thread_task, NULL) != 0)
      perror("Can't create client threads");
  }

  // Join all the threads before leaving
  // TODO
  for(i=0; i<num_threads; i++) {
    if(pthread_join(tid[i], NULL) != 0) 
      perror("Can't join client threads\n");
  }

  return 0;
}
