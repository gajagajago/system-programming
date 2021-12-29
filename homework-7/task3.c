//--------------------------------------------------------------------------------------------------
// System Programming                     Homework #7                                    Fall 2021
//
/// @file
/// @brief template
/// @author Ryu Junyul
/// @studid 2016-17097
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/// @brief program entry point
int main(int argc, char *argv[])
{
  char* exec_argv[] = {
    NULL,
    NULL,
    NULL,
  };

  if(strcmp(argv[1], "ls") == 0) {
    exec_argv[0] = "/bin/ls";
    
    if(argc == 3 && strcmp(argv[2], "-l") == 0)
      exec_argv[1] = "-l";
    
    execve(exec_argv[0], exec_argv, NULL);
  } else if (strcmp(argv[1], "cat") == 0) {
    exec_argv[0] = "/bin/cat";
    exec_argv[1] = argv[2];

    execve(exec_argv[0], exec_argv, NULL);
  } else {
    perror("Something went wrong\n");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
