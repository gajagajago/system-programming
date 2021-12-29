//--------------------------------------------------------------------------------------------------
// System Programming                         I/O Lab                                    Fall 2021
//
/// @file
/// @brief resursively traverse directory tree and list all entries
/// @author Junyul Ryu <gajagajago@snu.ac.kr>
/// @studid 2016-17097
//--------------------------------------------------------------------------------------------------

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <assert.h>
#include <grp.h>
#include <pwd.h>

#define MAX_DIR 64            ///< maximum number of directories supported

/// @brief output control flags
#define F_TREE      0x1       ///< enable tree view
#define F_SUMMARY   0x2       ///< enable summary
#define F_VERBOSE   0x4       ///< turn on verbose mode

/// @brief verbose format lengths
#define LEN_PATH_NAME 54
#define LEN_USER_NAME 8
#define LEN_GROUP_NAME 8
#define LEN_FILE_SIZE 10
#define LEN_DISK_BLOCKS 8
#define LEN_TYPE 1
#define LEN_TAB 2
#define LEN_SUMMARY 68
#define LEN_TOTAL_SIZE 14
#define LEN_TOTAL_BLOCKS 9

/// @brief struct holding the summary
struct summary {
  unsigned int dirs;          ///< number of directories encountered
  unsigned int files;         ///< number of files
  unsigned int links;         ///< number of links
  unsigned int fifos;         ///< number of pipes
  unsigned int socks;         ///< number of sockets

  unsigned long long size;    ///< total size (in bytes)
  unsigned long long blocks;  ///< total number of blocks (512 byte blocks)
};

/// @brief abort the program with EXIT_FAILURE and an optional error message
///
/// @param msg optional error message or NULL
void panic(const char *msg)
{
  if (msg) fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

/// @brief read next directory entry from open directory 'dir'. Ignores '.' and '..' entries
///
/// @param dir open DIR* stream
/// @retval entry on success
/// @retval NULL on error or if there are no more entries
struct dirent *getNext(DIR *dir)
{
  struct dirent *next;
  int ignore;

  do {
    errno = 0;
    next = readdir(dir);
    if (errno != 0) perror(NULL);
    ignore = next && ((strcmp(next->d_name, ".") == 0) || (strcmp(next->d_name, "..") == 0));
  } while (next && ignore);

  return next;
}


/// @brief qsort comparator to sort directory entries. Sorted by name, directories first.
///
/// @param a pointer to first entry
/// @param b pointer to second entry
/// @retval -1 if a<b
/// @retval 0  if a==b
/// @retval 1  if a>b
static int dirent_compare(const void *a, const void *b)
{
  struct dirent *e1 = (struct dirent*)a;
  struct dirent *e2 = (struct dirent*)b;

  // if one of the entries is a directory, it comes first
  if (e1->d_type != e2->d_type) {
    if (e1->d_type == DT_DIR) return -1;
    if (e2->d_type == DT_DIR) return 1;
  }

  // otherwise sorty by name
  return strcmp(e1->d_name, e2->d_name);
}

/// @brief add dots to path name if overflow
///
/// @param in path name
/// @retval ellipsed path name
char* ellipsis(const char* in) {
  char* out;

  if(strlen(in) > LEN_PATH_NAME) {
    char cpy[LEN_PATH_NAME] = {'\0'};
    const char* dots = "...";
    int len_cpy = (int)LEN_PATH_NAME - strlen(dots);

    strncpy(cpy, in, len_cpy);
    if(asprintf(&out, "%s%s", cpy, dots) == -1) panic("Out of memory\n");
  } else 
    if(asprintf(&out, "%s", in) == -1) panic("Out of memory\n");
  
  return out;
}

/// @brief get user name from user id
///
/// @param st_uid user id
/// @retval user name
char* getUserName(uid_t st_uid) {
  struct passwd* pwuid;

  if((pwuid = getpwuid(st_uid)) == NULL) panic("Error while finding user\n");

  return pwuid->pw_name;
}

/// @brief get group name from group id
///
/// @param st_gid group id
/// @retval group name
char* getGroupName(gid_t st_gid) {
  struct group* gr;

  if((gr = getgrgid(st_gid)) == NULL) panic("Error while finding group\n");

  return gr->gr_name;
}

/// @brief get verbose format path name
///
/// @param in raw path name
/// @length length of raw path name
/// @retval formatted path name
char* fmtPathName(const char* in, int length) {
  int min_l = ( length < (int)LEN_PATH_NAME ) ? length : LEN_PATH_NAME;
  char* ret;
  char blanks[(int)LEN_PATH_NAME];
  memset(blanks, ' ', (int)LEN_PATH_NAME);
  strncpy(blanks, in, min_l); 
  asprintf(&ret, "%s", blanks);

  return ret;
}

/// @brief prints verbose format user name
///
/// @param user raw user name
void fmtUser(char* user) {
  char u[(int)LEN_USER_NAME];
  int lu = (int)strlen(user) < (int)LEN_USER_NAME ? (int)strlen(user) : (int)LEN_USER_NAME;
 
  strncpy(u, user, (int)LEN_USER_NAME);

  for(int i=0; i<(int)LEN_USER_NAME-lu; i++)
    printf(" ");
  printf("%s",u);
}

/// @brief prints verbose format group name
///
/// @param group raw group name
void fmtGroup(char* group) {
  char g[(int)LEN_GROUP_NAME];
  int lg = (int)strlen(group) < (int)LEN_GROUP_NAME ? (int)strlen(group) : (int)LEN_GROUP_NAME;
 
  strncpy(g, group, (int)LEN_GROUP_NAME);
  printf("%s", g);

  for(int i=0; i<(int)LEN_GROUP_NAME-lg; i++)
    printf(" ");
}

/// @brief prints verbose format file size
///
/// @param size file size
void fmtFileSize(long long size) {
  char* s;
  asprintf(&s, "%lld", size);

  char u[(int)LEN_FILE_SIZE];
  int lu = (int)strlen(s) < (int)LEN_FILE_SIZE ? (int)strlen(s) : (int)LEN_FILE_SIZE;
 
  strncpy(u, s, (int)LEN_FILE_SIZE);
  free(s);
  for(int i=0; i<(int)LEN_FILE_SIZE-lu; i++)
    printf(" ");
  printf("%s",u);
}

/// @brief prints verbose format blocks count
///
/// @param blocks blocks count
void fmtBlocks(long long blocks) {
  char* s;
  asprintf(&s, "%lld", blocks);

  char u[(int)LEN_DISK_BLOCKS];
  int lu = (int)strlen(s) < (int)LEN_DISK_BLOCKS ? (int)strlen(s) : (int)LEN_DISK_BLOCKS;
 
  strncpy(u, s, (int)LEN_DISK_BLOCKS);
  free(s);
  for(int i=0; i<(int)LEN_DISK_BLOCKS-lu; i++)
    printf(" ");
  printf("%s",u);
}

/// @brief prints verbose format indent
void printIndent() {
  for(int i=0; i<(int)LEN_TAB; i++) {
    printf(" ");
  }
}

/// @brief prints summary format 
///
/// @param in summarized string
void fmtSummary(const char* in) {
  char g[(int)LEN_SUMMARY];
  int lg = (int)strlen(in) < (int)LEN_SUMMARY ? (int)strlen(in) : (int)LEN_SUMMARY;
 
  strncpy(g, in, (int)LEN_SUMMARY);
  printf("%s", g);

  for(int i=0; i<(int)LEN_SUMMARY-lg; i++)
    printf(" ");
}

/// @brief prints summary format total size 
///
/// @param size summarized total size
void fmtTotalSize(long long size) {
  char* s;
  asprintf(&s, "%lld", size);

  char u[(int)LEN_TOTAL_SIZE];
  int lu = (int)strlen(s) < (int)LEN_TOTAL_SIZE ? (int)strlen(s) : (int)LEN_TOTAL_SIZE;
 
  strncpy(u, s, (int)LEN_TOTAL_SIZE);
  free(s);
  for(int i=0; i<(int)LEN_TOTAL_SIZE-lu; i++)
    printf(" ");
  printf("%s",u);
}

/// @brief prints summary format total blocks 
///
/// @param in summarized total blocks count
void fmtTotalBlocks(long long blocks) {
  char* s;
  asprintf(&s, "%lld", blocks);

  char u[(int)LEN_TOTAL_BLOCKS];
  int lu = (int)strlen(s) < (int)LEN_TOTAL_BLOCKS ? (int)strlen(s) : (int)LEN_TOTAL_BLOCKS;
 
  strncpy(u, s, (int)LEN_TOTAL_BLOCKS);
  free(s);
  for(int i=0; i<(int)LEN_TOTAL_BLOCKS-lu; i++)
    printf(" ");
  printf("%s",u);
}

/// @brief make summary string from params. Call fmtSummary() to print the summary.
///
/// @param files files count
/// @param dirss directories count
/// @param links links count
/// @param fifos pipes count
/// @param socks sockets count
void printDirSummary(const int files, const int dirs, const int links, const int fifos, const int socks) {
  char* res;
  char* f = (files==1) ? "file" : "files";
  char* d = (dirs==1) ? "directory" : "directories";
  char* l = (links==1) ? "link" : "links";
  char* p = (fifos==1) ? "pipe" : "pipes";
  char* s = (socks==1) ? "socket" : "sockets";

  asprintf(&res, "%d %s, %d %s, %d %s, %d %s, and %d %s", files, f, dirs, d, links, l, fifos, p, socks, s);
  fmtSummary(res);
  free(res);
}

/// @brief recursively process directory @a dn and print its tree
///
/// @param dn absolute or relative path string
/// @param pstr prefix string printed in front of each entry
/// @param stats pointer to statistics
/// @param flags output control flags (F_*)
void processDir(const char *dn, const char *pstr, struct summary *stats, unsigned int flags)
{
  DIR *dir;
  int exit = 0;

  if((dir = opendir(dn)) == NULL) exit = 1;
 
  if(exit) {
    // Error processing directory
    printf("%s  %s\n", pstr, strerror(errno));
  } else {
    struct dirent *entry;
    struct dirent entries[4000]; // 4000 is temp big number

    unsigned int flg_t = (flags & F_TREE) != 0;
    unsigned int flg_s = (flags & F_SUMMARY) != 0;
    unsigned int flg_v = (flags & F_VERBOSE) != 0;

    unsigned int nent = 0;
    while((entry = getNext(dir)) != NULL) {
      entries[nent] = *entry;
      nent++;
    }

    qsort(&entries[0], nent, sizeof(entries[0]), dirent_compare);

    for(int i=0; i<nent; i++) {
      // Only -t
      if(flg_t && !flg_v) {
        char prefix = (i == nent-1) ? '`' : '|';
        char* pstr_ent;
        asprintf(&pstr_ent, "%s%c-%s",pstr,prefix, entries[i].d_name);  
        printf("%s", pstr_ent);

        free(pstr_ent);
      }

      // Only -v
      if(flg_v && !flg_t) {
        char* pstr_ent; 
        char* fmt_ent;
        char* ellipsis_ent;

        asprintf(&pstr_ent, "  %s%s", pstr, entries[i].d_name);
        ellipsis_ent = ellipsis(pstr_ent);
        fmt_ent = fmtPathName(ellipsis_ent, (int)strlen(ellipsis_ent));
        printf("%s", fmt_ent);

        free(pstr_ent);
        free(fmt_ent);
        free(ellipsis_ent);
      }

      // -t -v
      if(flg_t && flg_v) {
        char prefix = (i == nent-1) ? '`' : '|';
        char* pstr_ent;
        char* fmt_ent;
        char* ellipsis_ent;

        asprintf(&pstr_ent, "%s%c-%s",pstr,prefix, entries[i].d_name); 
        ellipsis_ent = ellipsis(pstr_ent);
        fmt_ent = fmtPathName(ellipsis_ent, (int)strlen(ellipsis_ent));
        printf("%s", fmt_ent);

        free(pstr_ent);
        free(fmt_ent);
        free(ellipsis_ent);
      }

      // No option
      if(!flg_t && !flg_v) {
        printf("  %s%s",pstr, entries[i].d_name);
      }

      // 2 indent after pathname
      printIndent();

      struct dirent* this = &entries[i];
      char *cent;

      if(asprintf(&cent, "%s%s%s", dn, "/", this->d_name) == -1)
        panic("Out of memory\n");

      int flg_continue = 1; // flg false when stat() error to stop processing dir
      // stat for -v, -s
      if(flg_v || flg_s) {

        struct stat sb;
        if ( (lstat(cent, &sb) == -1) && (stat(cent, &sb) == -1) ) {
          if(flg_v) printf("%s", strerror(errno));
          flg_continue = 0;
        } else {
          char f_type;
          
          switch (sb.st_mode & S_IFMT) { 
            case S_IFBLK: f_type='b'; break; 
            case S_IFCHR: f_type='c'; break; 
            case S_IFDIR: f_type='d'; stats->dirs+=1; break; 
            case S_IFIFO: f_type='f'; stats->fifos+=1; break; 
            case S_IFLNK: f_type='l'; stats->links+=1; break; 
            case S_IFREG: f_type='\0'; stats->files+=1; break;
            case S_IFSOCK: f_type='s'; stats->socks+=1; break; 
            default: f_type='\0'; break;
          }

          // add fsize and blks to stats
          stats->size += sb.st_size;
          stats->blocks += sb.st_blocks;

          if(flg_v) {
            // Print user:group
            char* user_name = getUserName(sb.st_uid);
            fmtUser(user_name);
            printf(":");
            char* group_name = getGroupName(sb.st_gid);
            fmtGroup(group_name);

            // 2 indent after user:group
            printIndent();
        
            // Print file size 
            long long size_bytes = (long long) sb.st_size;
            fmtFileSize(size_bytes);

            // 2 indent after file size
            printIndent();

            long long blocks = (long long) sb.st_blocks;
            fmtBlocks(blocks);

            // 2 indent after blocks
            printIndent();
    
            // Print type
            printf("%c", f_type);
  
            // 2 Indent after type
            printIndent();
          }
        }
      }

      printf("\n"); // final newline

      if(flg_continue && (this->d_type == DT_DIR)) {
        char* newPstr;
      
        if(flg_t) {
          if(asprintf(&newPstr, "| %s", pstr) == -1) panic("Out of memory\n");
        } else {
          if(asprintf(&newPstr, "  %s", pstr) == -1)panic("Out of memory\n");
        }

        processDir(cent, newPstr, stats, flags); 
      }
    }

    // close open directory
    if(closedir(dir) == -1) panic("Error while closing directory");
  }
}


/// @brief print program syntax and an optional error message. Aborts the program with EXIT_FAILURE
///
/// @param argv0 command line argument 0 (executable)
/// @param error optional error (format) string (printf format) or NULL
/// @param ... parameter to the error format string
void syntax(const char *argv0, const char *error, ...)
{
  if (error) {
    va_list ap;

    va_start(ap, error);
    vfprintf(stderr, error, ap);
    va_end(ap);

    printf("\n\n");
  }

  assert(argv0 != NULL);

  fprintf(stderr, "Usage %s [-t] [-s] [-v] [-h] [path...]\n"
                  "Gather information about directory trees. If no path is given, the current directory\n"
                  "is analyzed.\n"
                  "\n"
                  "Options:\n"
                  " -t        print the directory tree (default if no other option specified)\n"
                  " -s        print summary of directories (total number of files, total file size, etc)\n"
                  " -v        print detailed information for each file. Turns on tree view.\n"
                  " -h        print this help\n"
                  " path...   list of space-separated paths (max %d). Default is the current directory.\n",
                  basename(argv0), MAX_DIR);

  exit(EXIT_FAILURE);
}


/// @brief program entry point
int main(int argc, char *argv[])
{
  //
  // default directory is the current directory (".")
  //
  const char CURDIR[] = ".";
  const char *directories[MAX_DIR];
  int   ndir = 0;

  struct summary dstat, tstat;
  unsigned int flags = 0;

  //
  // parse arguments
  //
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      // format: "-<flag>"
      if      (!strcmp(argv[i], "-t")) flags |= F_TREE;
      else if (!strcmp(argv[i], "-s")) flags |= F_SUMMARY;
      else if (!strcmp(argv[i], "-v")) flags |= F_VERBOSE;
      else if (!strcmp(argv[i], "-h")) syntax(argv[0], NULL);
      else syntax(argv[0], "Unrecognized option '%s'.", argv[i]);
    } else {
      // anything else is recognized as a directory
      if (ndir < MAX_DIR) {
        directories[ndir++] = argv[i];
      } else {
        printf("Warning: maximum number of directories exceeded, ignoring '%s'.\n", argv[i]);
      }
    }
  }

  // if no directory was specified, use the current directory
  if (ndir == 0) directories[ndir++] = CURDIR;


  //
  // process each directory
  //
  // TODO
  //

  // Verbose mode header formatting
  unsigned int flg_v = (flags & F_VERBOSE) != 0;
  unsigned int flg_s = (flags & F_SUMMARY) != 0;

  memset(&tstat, 0, sizeof(tstat));

  for (int i=0; i<ndir; i++) {

    if (flg_v) {
      printf("Name");
      for(int i =5; i<=60; i++) printf(" ");
      printf("User:Group");
      for(int i =71; i<=81; i++) printf(" ");
      printf("Size");
      for(int i =86; i<=89; i++) printf(" ");
      printf("Blocks Type\n");
      for(int i =1; i<=100; i++) printf("-");
      printf("\n");
    }

    printf("%s\n", directories[i]); 
    memset(&dstat, 0, sizeof(dstat));
    processDir(directories[i],"", &dstat, flags);

    // Print directory summary
    if (flg_s) {
      for(int i =1; i<=100; i++) printf("-");
      printf("\n");

      // directory summary
      printDirSummary(dstat.files, dstat.dirs, dstat.links, dstat.fifos, dstat.socks);
      printf("   ");

      if (flg_v) {
        fmtTotalSize(dstat.size);
        printf(" ");
        fmtTotalBlocks(dstat.blocks);
      }

      printf("\n");
      // Accumulate tstat if ndir>1
      if(ndir>1) {
        tstat.files += dstat.files;
        tstat.dirs += dstat.dirs;
        tstat.links += dstat.links;
        tstat.fifos += dstat.fifos;
        tstat.socks += dstat.socks;
        tstat.size += dstat.size;
        tstat.blocks += dstat.blocks;
      }
    }

    // \n after traversing each dir if ndir != 1
    if (ndir >1) printf("\n");
  }


  //
  // print grand total
  //
  if ((flags & F_SUMMARY) && (ndir > 1)) {
    printf("Analyzed %d directories:\n"
           "  total # of files:        %16d\n"
           "  total # of directories:  %16d\n"
           "  total # of links:        %16d\n"
           "  total # of pipes:        %16d\n"
           "  total # of socksets:     %16d\n",
           ndir, tstat.files, tstat.dirs, tstat.links, tstat.fifos, tstat.socks);

    if (flags & F_VERBOSE) {
      printf("  total file size:         %16llu\n"
             "  total # of blocks:       %16llu\n",
             tstat.size, tstat.blocks);
    }

  }

  //
  // that's all, folks
  //
  return EXIT_SUCCESS;
}
