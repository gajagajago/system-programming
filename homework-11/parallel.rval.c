#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAXTHREAD 256

typedef struct _tdata {
  pthread_t tid;
  void *data;
  off_t from, to;
  unsigned char result;
} ThreadData;

#define SYNTAX(err) syntax(argv[0], err);

void ABORT(char *error)
{
  if (error) printf("Error: %s\n", error);
  abort();
}

void syntax(char *progname, char *error)
{
  if (error) printf("Error: %s\n\n", error);

  printf("Compute 8-bit CRC for a file using n threads.\n"
         "\n"
         "Syntax: %s <filename> <threads>\n"
         "\n"
         "  <filename>    name of file\n"
         "  <threads>     number of threads (1-%d)\n"
         "\n",
         basename(progname), MAXTHREAD);

  abort();
}

//
// Compute a 8-bit cyclic redundancy check for a memory-mapped file 
// from position 'pos' (including) to 'end' (exluding).
//
void* crc8(void *argp)
{
  // retrieve parameters
  ThreadData *td = (ThreadData*)argp;

  char *data = td->data;
  off_t from = td->from;
  off_t to   = td->to;

  // compute CRC from [from - to)
  unsigned char crc8 = 0;
  while (from < to) crc8 ^= data[from++];

  // TODO return value to main thread & compute global crc there
  td->result = crc8;

  return NULL;
}


int main(int argc, char *argv[])
{
  //
  // got three arguments?
  //
  if (argc != 3) SYNTAX("Missing arguments.");

  char *fn = argv[1];
  int nthread= atoi(argv[2]);

  if ((nthread < 1) || (nthread > MAXTHREAD)) 
    SYNTAX("Invalid number of threads.");


  //
  // open file, get its size, and map it into our memory space
  //
  int fd = open(fn, O_RDONLY);
  if (fd == -1) ABORT(strerror(errno));

  struct stat s;
  if (fstat(fd, &s) < 0) ABORT(strerror(errno));

  char *filedata = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (filedata == NULL) ABORT(strerror(errno));

  //
  // make sure we do not create more threads than there are bytes in the file
  //
  if (nthread > s.st_size) nthread = s.st_size;

  //
  // create 'nthread' threads, each computing a part of the CRC
  //
  ThreadData *td = malloc(nthread * sizeof(ThreadData)); assert(td != NULL);

  // TODO
  int chunkbytes = s.st_size / nthread;
  int from = 0;
  int to = chunkbytes;

  for(int i=0; i<nthread; i++) {
    td[i].data = filedata;
    
    // Last iteration covers chunkbytes + residuals
    if(i == nthread-1) 
      to = s.st_size;

    //printf("Iter[%d] from: %d to: %d  nbytes:%d\n", i, from, to, to-from);
    td[i].from = from;
    td[i].to = to;

    if(pthread_create(&td[i].tid, NULL, crc8, &td[i]) != 0)
      ABORT(strerror(errno));

    from += chunkbytes;
    to += chunkbytes;
  }
  //
  // wait for all threads to end and sum up their result
  //
  // TODO
  unsigned char crc = 0;

  for(int i=0; i<nthread; i++) {
    if(pthread_join(td[i].tid, NULL) != 0)
      ABORT(strerror(errno));
    crc ^= td[i].result;
  }
  //
  // free resources
  //
  free(td);
  munmap(filedata, s.st_size);
  close(fd);

  //
  // print result
  //
  printf("CRC8(%s, %d) = %02x\n", fn, nthread, crc);

  //
  // that's all, folks!
  //
  return EXIT_SUCCESS;
}
