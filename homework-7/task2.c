//--------------------------------------------------------------------------------------------------
// System Programming                     Homework #7                                    Fall 2021
//
/// @file
/// @brief template
/// @author Ryu Junyul
/// @studid 2016-17097
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

// Function called by parent process
void parent() {
  printf("[%d] Hello from parent.\n", getpid());
  printf("[%d]  Waiting for child to terminate...\n", getpid());
}

int count = 0;
void hdl_sigusr1() {
  count++;
  printf("[%d] Child received SIGUSR1! Count = %d.\n",getpid(), count);
}

void hd1_sigusr2() {
  printf("[%d] Child received SIGUSR2! Will terminate soon..\n", getpid());
  exit(count);
}

// Function called by child process 
void child() {
  printf("[%d] Hello from child.\n", getpid());

  // Install 2 handlers
  struct sigaction action1;
  struct sigaction action2;

  action1.sa_handler = hdl_sigusr1;
  printf("[%d]  SIGUSR1 handler installed.\n", getpid());
  action2.sa_handler = hd1_sigusr2;
  printf("[%d]  SIGUSR2 handler installed.\n", getpid());

  action1.sa_flags = SA_RESTART;

  if(sigaction(SIGINT, &action1, NULL) == -1)
    exit(EXIT_FAILURE);
  if(sigaction(SIGTERM, &action2, NULL) == -1)
    exit(EXIT_FAILURE);

  while(1);
}

/// @brief program entry point
int main(int argc, char *argv[])
{
  pid_t pid = fork();

  if(pid == -1) {
    printf("Failed to fork\n");
    return EXIT_FAILURE;
  } 

  if(pid == 0) {
    child();
  } else {
    parent();
    
    int wstatus;
    wait(&wstatus);
    
    if(WIFEXITED(wstatus)) {
      printf("[%d] Child has terminated normally. It has received %d SIGUSR1 signals.\n", getpid(), WEXITSTATUS(wstatus));
    }
  }

  return EXIT_SUCCESS;
}
