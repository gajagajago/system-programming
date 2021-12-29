//--------------------------------------------------------------------------------------------------
// System Programming                     Homework #8                                    Fall 2021
//
/// @file
/// @brief Summarize size of files in a directory tree
/// @author <yourname>
/// @studid <studentid>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ 0
#define WRITE 1

void child(int pfd[2], char* dir) {
  // Close unnecessary pfd
  close(pfd[READ]);
  
  // Redirect stdout to pipe write 
  if(dup2(pfd[WRITE], STDOUT_FILENO) == -1) {
    printf("Error in dup2()\n");
    exit(EXIT_FAILURE);
  }

  char* exec_argv[] = {
    "find",
    dir,
    "-type",
    "f",
    "-printf",
    "%s %f\n",
    NULL,
  };

  execvp(exec_argv[0], exec_argv);
}

void parent(int pfd[2]) {
  // Close unnecessary pfd
  close(pfd[WRITE]);

  // Open file stream at read end
  FILE* fs;
  if((fs = fdopen(pfd[READ], "r")) == NULL) {
    printf("Error in fdopen()\n");
    exit(EXIT_FAILURE);
  }
  
  int totalFiles = 0;
  int totalSize = 0;
  char* biggestFile;
  int biggestSize = 0;
  int size;
  char* name;
  while(fscanf(fs, "%d %ms", &size, &name) != EOF) {
    totalFiles ++;
    totalSize += size;
    
    if(size > biggestSize) {
      biggestSize = size;
      if(asprintf(&biggestFile, "%s", name) == -1) {
        printf("Error in asprintf()\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  printf("Found %d files with a total size of %d bytes.\n", totalFiles, totalSize);
  printf("The largest file is '%s' with a size of %d bytes.\n", biggestFile, biggestSize);
  
  free(biggestFile);
  fclose(fs);
}
/// @brief program entry point
int main(int argc, char *argv[])
{
  char *dir = (argc > 1 ? argv[1] : ".");

  int pfd[2];
  pid_t pid;

  // Create a pipe
  if (pipe(pfd) == -1) {
    printf("Error in pipe()\n");
    exit(EXIT_FAILURE);
  }

  if ((pid = fork()) == -1) {
    printf("Error in fork()\n");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {
    child(pfd, dir);
  } else {
    parent(pfd);
  }
  //
  // That's all, folks!
  //
  return EXIT_SUCCESS;
}
