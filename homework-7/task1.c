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

// Function called by parent process
void parent(pid_t pid) {
  printf("[%d] Hello from parent.\n", pid);
}
// Function called by child process 
void child(pid_t pid) {
  printf("[%d] Hello from child.\n", pid);
}
/// @brief program entry point
int main(int argc, char *argv[])
{
  pid_t pid = fork();

  if(pid == -1) {
    printf("Failed to fork\n");
    return EXIT_FAILURE;
  } else {
    if(pid == 0)
      child(getpid());
    else 
      parent(getpid());
  }

  return EXIT_SUCCESS;
}
