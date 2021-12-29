#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAXPROC 16

// abort process with an optional error message
void ABORT(char *msg)
{
  if (msg) { printf("%s\n", msg); fflush(stdout); }
  abort();
}

int main(int argc, char *argv[])
{
  //
  // convert argument at command line into number
  //
  if (argc != 2) ABORT("Missing argument.");
  int nproc = atoi(argv[1]);

  if (nproc < 1) nproc = 1;
  if (nproc > MAXPROC) nproc = MAXPROC;

  //
  // TODO
  //
  pid_t pid;

  for(int i=1; i<=nproc; i++) {
    if((pid = fork()) == -1)
      ABORT("Failed to fork.\n");

    if (pid == 0) {
      char* n;

      char* exec_argv[] = {
        "child",
        NULL,
        NULL,
      };

      asprintf(&n, "%d", i);
      exec_argv[1] = n;
      execve(exec_argv[0], exec_argv, NULL);
      free(n);
    }
  }

  if (pid > 0) {
      int wstatus;

      while(nproc > 0) {
        int fid = wait(&wstatus);

        if(WIFEXITED(wstatus)) {
          printf("Child [%d] terminated normally with exit code %d.\n", fid, WEXITSTATUS(wstatus));
          nproc--;
        }
    
      }
  }

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}
