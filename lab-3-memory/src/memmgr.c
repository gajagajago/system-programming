//--------------------------------------------------------------------------------------------------
// System Programming                       Memory Lab                                   Fall 2021
//
/// @file
/// @brief dynamic memory manager
/// @author Ryu Junyul
/// @studid 2016-17097
//--------------------------------------------------------------------------------------------------


// Dynamic memory manager
// ======================
// This module implements a custom dynamic memory manager.
//
// Heap organization:
// ------------------
// The data segment for the heap is provided by the dataseg module. A 'word' in the heap is
// eight bytes.
//
// Implicit free list:
// -------------------
// - minimal block size: 32 bytes (header +footer + 2 data words)
// - h,f: header/footer of free block
// - H,F: header/footer of allocated block
//
// - state after initialization
//
//         initial sentinel half-block                  end sentinel half-block
//                   |                                             |
//   ds_heap_start   |   heap_start                         heap_end       ds_heap_brk
//               |   |   |                                         |       |
//               v   v   v                                         v       v
//               +---+---+-----------------------------------------+---+---+
//               |???| F | h :                                 : f | H |???|
//               +---+---+-----------------------------------------+---+---+
//                       ^                                         ^
//                       |                                         |
//               32-byte aligned                           32-byte aligned
//
// - allocation policies: first, next, best fit
// - block splitting: always at 32-byte boundaries
// - immediate coalescing upon free
//


#include <assert.h>
#include <error.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "dataseg.h"
#include "memmgr.h"
#include <limits.h>
#include <stdbool.h>

void mm_check(void);

/// @name global variables
/// @{
static void *ds_heap_start = NULL;                     ///< physical start of data segment
static void *ds_heap_brk   = NULL;                     ///< physical end of data segment
static void *heap_start    = NULL;                     ///< logical start of heap
static void *heap_end      = NULL;                     ///< logical end of heap
static int  PAGESIZE       = 0;                        ///< memory system page size
static void *(*get_free_block)(size_t) = NULL;         ///< get free block for selected allocation policy
static int  mm_initialized = 0;                        ///< initialized flag (yes: 1, otherwise 0)
static int  mm_loglevel    = 0;                        ///< log level (0: off; 1: info; 2: verbose)
static void* nf_ptr        = NULL;                     ///< global pointer for next fit allocation policy
static bool nf             = false;                    ///< whether ap is next fit>
/// @}

/// @name Macro definitions
/// @{
#define MAX(a, b)          ((a) > (b) ? (a) : (b))     ///< MAX function

#define TYPE               unsigned long               ///< word type of heap
#define TYPE_SIZE          sizeof(TYPE)                ///< size of word type

#define ALLOC              1                           ///< block allocated flag
#define FREE               0                           ///< block free flag
#define STATUS_MASK        ((TYPE)(0x7))               ///< mask to retrieve flagsfrom header/footer
#define SIZE_MASK          (~STATUS_MASK)              ///< mask to retrieve size from header/footer

#define CHUNKSIZE          (1*(1 << 12))               ///< size by which heap is extended

#define BS                 32                          ///< minimal block size. Must be a power of 2
#define BS_MASK            (~(BS-1))                   ///< alignment mask

#define WORD(p)            ((TYPE)(p))                 ///< convert pointer to TYPE
#define PTR(w)             ((void*)(w))                ///< convert TYPE to void*

#define PREV_PTR(p)        ((p)-TYPE_SIZE)             ///< get pointer to word preceeding p
#define NEXT_PTR(p)        ((p)+TYPE_SIZE)             ///< get pointer to word following p
#define HDRP(bp)           (PREV_PTR(bp))              ///< get pointer to header 
#define FTRP(bp)           PTR(WORD(HDRP(bp))+GET_SIZE(HDRP(bp))-TYPE_SIZE) ///< get pointer to footer 

#define PREV_BLK_PTR(bp)   (bp-GET_SIZE(PREV_PTR(HDRP(bp)))) //< get block pointer of previous block
#define NEXT_BLK_PTR(bp)   (bp+GET_SIZE(HDRP(bp)))      //< get block pointer of next block

#define PACK(size,status)  ((size) | (status))         ///< pack size & status into boundary tag
#define SIZE(v)            (v & SIZE_MASK)             ///< extract size from boundary tag
#define STATUS(v)          (v & STATUS_MASK)           ///< extract status from boundary tag

#define GET(p)             (*(TYPE*)(p))               ///< read word at *p
#define GET_SIZE(p)        (SIZE(GET(p)))              ///< extract size from header/footer
#define GET_STATUS(p)      (STATUS(GET(p)))            ///< extract status from header/footer

#define PUT(p,val)         (*(TYPE*)(p) = (val))       ///< write word at *p> 
                 
// TODO add more macros as needed

/// @brief print a log message if level <= mm_loglevel. The variadic argument is a printf format
///        string followed by its parametrs
#ifdef DEBUG
  #define LOG(level, ...) mm_log(level, __VA_ARGS__)

/// @brief print a log message. Do not call directly; use LOG() instead
/// @param level log level of message.
/// @param ... variadic parameters for vprintf function (format string with optional parameters)
static void mm_log(int level, ...)
{
  if (level > mm_loglevel) return;

  va_list va;
  va_start(va, level);
  const char *fmt = va_arg(va, const char*);

  if (fmt != NULL) vfprintf(stdout, fmt, va);

  va_end(va);

  fprintf(stdout, "\n");
}

#else
  #define LOG(level, ...)
#endif

/// @}


/// @name Program termination facilities
/// @{

/// @brief print error message and terminate process. The variadic argument is a printf format
///        string followed by its parameters
#define PANIC(...) mm_panic(__func__, __VA_ARGS__)

/// @brief print error message and terminate process. Do not call directly, Use PANIC() instead.
/// @param func function name
/// @param ... variadic parameters for vprintf function (format string with optional parameters)
static void mm_panic(const char *func, ...)
{
  va_list va;
  va_start(va, func);
  const char *fmt = va_arg(va, const char*);

  fprintf(stderr, "PANIC in %s%s", func, fmt ? ": " : ".");
  if (fmt != NULL) vfprintf(stderr, fmt, va);

  va_end(va);

  fprintf(stderr, "\n");

  exit(EXIT_FAILURE);
}

static void* ff_get_free_block(size_t);
static void* nf_get_free_block(size_t);
static void* bf_get_free_block(size_t);

/// @brief initialize heap space
void mm_init(AllocationPolicy ap)
{
  LOG(1, "mm_init()");

  //
  // set allocation policy
  //
  char *apstr;
  switch (ap) {
    case ap_FirstFit: get_free_block = ff_get_free_block; apstr = "first fit"; break;
    case ap_NextFit:  get_free_block = nf_get_free_block; apstr = "next fit";  break;
    case ap_BestFit:  get_free_block = bf_get_free_block; apstr = "best fit";  break;
    default: PANIC("Invalid allocation policy.");
  }
  LOG(2, "  allocation policy       %s\n", apstr);

  //
  // retrieve heap status and perform a few initial sanity checks
  //
  ds_heap_stat(&ds_heap_start, &ds_heap_brk, NULL);
  PAGESIZE = ds_getpagesize();

  LOG(2, "  ds_heap_start:          %p\n"
         "  ds_heap_brk:            %p\n"
         "  PAGESIZE:               %d\n",
         ds_heap_start, ds_heap_brk, PAGESIZE);

  if (ds_heap_start == NULL) PANIC("Data segment not initialized.");
  if (ds_heap_start != ds_heap_brk) PANIC("Heap not clean.");
  if (PAGESIZE == 0) PANIC("Reported pagesize == 0.");

  // Initialize heap
  if(ds_sbrk(CHUNKSIZE) == (void*)-1)
      PANIC("ds_sbrk in mm_init");

  // Get ds heap stat
  ds_heap_stat(&ds_heap_start, &ds_heap_brk, NULL);

  // Align heap relative to ds heap stat
  heap_start = PTR((WORD(ds_heap_start)/BS + 1) * BS);
  heap_end = PTR((WORD(ds_heap_brk)/BS -1) * BS);
  
  // Set sentinels
  PUT(PREV_PTR(heap_start), PACK(0, ALLOC));
  PUT(heap_end, PACK(0, ALLOC));

  // Free the entire heap
  PUT(heap_start, PACK(heap_end - heap_start, FREE));
  PUT(PREV_PTR(heap_end), PACK(heap_end - heap_start, FREE));

  // Initialize next fit pointer if allocate policy is next fit
  if(ap == ap_NextFit) {
    nf = true;
    nf_ptr = heap_start;
  }

  // heap is initialized
  mm_initialized = 1;
}

/// @brief coalesce blocks if multiple free blocks are adjacent
///
/// @param bp block pointer for coalesce target block 
/// @retval bp block pointer for coalesced block 
void* coalesce(void* bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  LOG(1, "Coalesce: %p, Size: %d\n", bp, size);

  void* prev_blk_ftrp = PREV_PTR(HDRP(bp));
  void* next_blk_hdrp = NEXT_PTR(FTRP(bp));

  size_t prev_alloc = GET_STATUS(prev_blk_ftrp);
  size_t next_alloc = GET_STATUS(next_blk_hdrp);

  if (prev_alloc && !next_alloc) {
    size += GET_SIZE(next_blk_hdrp);
    PUT(HDRP(bp), PACK(size, FREE));
    PUT(FTRP(bp), PACK(size, FREE)); 
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(prev_blk_ftrp);
    PUT(HDRP(PREV_BLK_PTR(bp)), PACK(size, FREE));
    PUT(FTRP(bp), PACK(size, FREE));
    bp = PREV_BLK_PTR(bp);
  } else if (!prev_alloc && !next_alloc) {
    size += GET_SIZE(prev_blk_ftrp) + GET_SIZE(next_blk_hdrp);
    PUT(HDRP(PREV_BLK_PTR(bp)), PACK(size, FREE));
    PUT(FTRP(NEXT_BLK_PTR(bp)), PACK(size, FREE));
    bp = PREV_BLK_PTR(bp);
  }

  return bp;
}

/// @brief extend heap for additional memory space
/// 
/// @param size extend size
/// @retval block pointer to extended block
void* extend_heap(size_t size) 
{
  void* old_heap_brk;

  if((old_heap_brk = ds_sbrk(size)) == (void*)-1)
    PANIC("Error when extend heap");
 
  // 1. Free old heap end block
  size_t old_end_sentinel_size = old_heap_brk - heap_end; 
  PUT(heap_end, PACK(old_end_sentinel_size, FREE));
  PUT(FTRP(NEXT_PTR(heap_end)), PACK(old_end_sentinel_size, FREE));

  // 2. Reset ds stat
  ds_heap_stat(&ds_heap_start, &ds_heap_brk, NULL);

  // 3. Initialize new heap block
  // Place heap end at ds_heap_brk - 4 words since heap is already aligned
  PUT(ds_heap_brk - 4*TYPE_SIZE, PACK(0, ALLOC));
  heap_end = ds_heap_brk - 4*TYPE_SIZE;

  size_t added_block_size = WORD(heap_end - old_heap_brk);
  PUT(old_heap_brk, PACK(added_block_size, FREE));
  PUT(FTRP(NEXT_PTR(old_heap_brk)), PACK(added_block_size, FREE));

  // 4. Coalesce previous end sentinels
  return coalesce(PREV_BLK_PTR(NEXT_PTR(old_heap_brk)));
}

/// @brief allocate memory space
///
/// @param size minumum requested size
/// @retval block pointer to allocated block
void* mm_malloc(size_t size)
{
  LOG(1, "mm_malloc(0x%lx)", size);

  assert(mm_initialized);
  
  // Calculate allocated block size
  size_t alloc_size = ceil( (double)(size + 2*TYPE_SIZE) / BS ) * BS;
  void* addr = get_free_block(alloc_size);

  // If addr == Null, there's no big enough free blk.
  // Extend heap "until" bp is allocated.
  while(addr == NULL) {
    extend_heap(CHUNKSIZE);
    addr = get_free_block(alloc_size);
  } 

  size_t actual_alloc_size = GET_SIZE(addr);
  // Set header, footer for allocated blk
  PUT(addr, PACK(alloc_size, ALLOC));
  PUT(FTRP(NEXT_PTR(addr)), PACK(alloc_size, ALLOC));

  // Check if size is bigger than requested size
  // if bigger, split the block
  if(actual_alloc_size > alloc_size){
    void* split_blk = addr + alloc_size;

    PUT(split_blk, PACK(actual_alloc_size - alloc_size, FREE));
    PUT(FTRP(NEXT_PTR(split_blk)), PACK(actual_alloc_size - alloc_size, FREE));
  }

  return NEXT_PTR(addr);
}

/// @brief allocate 0 initialized memory space
///
/// @param n_memb number of elements in list
/// @param size size of each element 
/// @retval block pointer for allocated block
void* mm_calloc(size_t nmemb, size_t size)
{
  LOG(1, "mm_calloc(0x%lx, 0x%lx)", nmemb, size);

  assert(mm_initialized);

  if(nmemb == 0 || size == 0)
    return NULL;

  if(UINT_MAX / nmemb < size)
    PANIC("Arithmetic overflow while mm_calloc\n");

  void *payload = mm_malloc(nmemb * size);

  if (payload != NULL) memset(payload, 0, nmemb * size);

  return payload;
}

/// @brief resize allocated block
///
/// @param bp block pointer to target allocated block
/// @param size new size for bp
/// @retval block pointer for resized allocated block
void* mm_realloc(void *bp, size_t size)
{
  LOG(2, "mm_realloc(%p, 0x%lx)", bp, size);

  assert(mm_initialized);

  if(bp == NULL)
    bp = mm_malloc(size);
  if(size == 0)
    mm_free(bp);

  void* org_hdrp = HDRP(bp);
  size_t org_size = GET_SIZE(org_hdrp);

  // 1. Calculate alloc size
  size_t alloc_size = ceil( (double)(size + 2*TYPE_SIZE) / BS ) * BS;

  // 2. Deal w/ 3 cases: 
  if(alloc_size < org_size) {
    // 1. Set new border tag for bp
    PUT(org_hdrp, PACK(alloc_size, ALLOC));
    PUT(FTRP(bp), PACK(alloc_size, ALLOC));
    // 2. Set border tag for splitted blk
    void* next_bp = NEXT_BLK_PTR(bp);
    PUT(HDRP(next_bp), PACK(org_size - alloc_size, FREE));
    PUT(FTRP(next_bp), PACK(org_size - alloc_size, FREE));
    // 3. Coalesce splitted blk
    coalesce(next_bp);
  } else if(alloc_size > org_size) {
    // 1. Check next block header
    void* next_bp = NEXT_BLK_PTR(bp);
    size_t next_blk_size = GET_SIZE(HDRP(next_bp));

    // Next block is free and enough
    if(GET_STATUS(HDRP(next_bp)) == FREE && next_blk_size >= (alloc_size-org_size)) {
      // 1. Update org border tag
      PUT(org_hdrp, PACK(alloc_size, ALLOC));
      PUT(FTRP(bp), PACK(alloc_size, ALLOC));

      // 2. If next block was bigger, split the block and set border tag
      if(next_blk_size > alloc_size-org_size) {
        void* splitted_bp = NEXT_BLK_PTR(bp);
        size_t splitted_blk_size = next_blk_size - (alloc_size-org_size);
  
        PUT(HDRP(splitted_bp), PACK(splitted_blk_size, FREE));
        PUT(FTRP(splitted_bp), PACK(splitted_blk_size, FREE));
      }
    } else {
      // Next block is alloc or not enough
      // Find new block 
      void* new_bp = mm_malloc(size);
      size_t payload_size = org_size - 2*TYPE_SIZE;
      mm_free(bp);
      bp = memcpy(new_bp, bp, payload_size); // memcpy returns ptr to dest
    }
  }
  // return bp if alloc_size == org_size

  return bp;
}

/// @brief free allocated block
///
/// @param bp block pointer for free target block
void mm_free(void *bp)
{
  LOG(1, "mm_free(%p)", bp);

  assert(mm_initialized);

  if(bp == NULL) 
    return;
  
  // Reset nf_ptr to heap start if freeing block is nf pointed block
  if(nf && (HDRP(bp) == nf_ptr)) {
    LOG(1, "Freeing nf_pointed block. Remove nf_ptr to heap start\n");
    nf_ptr = heap_start;
  }

  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, FREE));
  PUT(FTRP(bp), PACK(size, FREE));

  coalesce(bp);
}

/// @name block allocation policites
/// @{

/// @brief find and return a free block of at least @a size bytes (first fit)
/// @param size size of block (including header & footer tags), in bytes
/// @retval void* pointer to header of large enough free block
/// @retval NULL if no free block of the requested size is avilable
static void* ff_get_free_block(size_t size)
{
  LOG(1, "ff_get_free_block(1x%lx (%lu))", size, size);

  assert(mm_initialized);
  
  void* ret_ptr = NULL;
  void* traverse_ptr = heap_start;
   
  while(traverse_ptr < heap_end) {
    unsigned long value = GET_SIZE(traverse_ptr);
    unsigned long flag = GET_STATUS(traverse_ptr);

    // Found big enough free block
    if (flag == FREE && value >= size){
      ret_ptr = traverse_ptr;
      break;
    } else {
      traverse_ptr += value;
    }
  }

  return ret_ptr;
}

/// @brief find and return a free block of at least @a size bytes (next fit)
/// @param size size of block (including header & footer tags), in bytes
/// @retval void* pointer to header of large enough free block
/// @retval NULL if no free block of the requested size is avilable
static void* nf_get_free_block(size_t size)
{
  LOG(1, "nf_get_free_block(0x%x (%lu))", size, size);

  assert(mm_initialized);

  void* ret_ptr = NULL;
  void* traverse_ptr = nf_ptr;

  do
  {
    // Return to heap start if traverse_ptr arrives at heap end
    if(traverse_ptr == heap_end) {
      traverse_ptr = heap_start;
      continue;
    }

    unsigned long value = GET_SIZE(traverse_ptr);
    unsigned long flag = GET_STATUS(traverse_ptr);

    // Found big enough free block
    if (flag == FREE && value >= size){
      ret_ptr = traverse_ptr;
      nf_ptr = traverse_ptr;
      break;
    } else {
      traverse_ptr += value;
    }

  } while(traverse_ptr != nf_ptr);

  return ret_ptr;
}

/// @brief find and return a free block of at least @a size bytes (best fit)
/// @param size size of block (including header & footer tags), in bytes
/// @retval void* pointer to header of large enough free block
/// @retval NULL if no free block of the requested size is avilable
static void* bf_get_free_block(size_t size)
{
  LOG(1, "bf_get_free_block(0x%lx (%lu))", size, size);

  assert(mm_initialized);

  void* bf_ptr = NULL;
  size_t bf_fragmentation = UINT_MAX;
  void* traverse_ptr = heap_start;

  while(traverse_ptr < heap_end) {
    unsigned long value = GET_SIZE(traverse_ptr);
    unsigned long flag = GET_STATUS(traverse_ptr);

    if(flag == FREE && value >= size) {
      size_t fragmentation = value - size;

      if(fragmentation < bf_fragmentation) {
        bf_ptr = traverse_ptr;
        bf_fragmentation = fragmentation;
      }
    }

    traverse_ptr += value;
  }

  return bf_ptr;
}

/// @brief set log level for mm_log
///
/// @param level log level to set
void mm_setloglevel(int level)
{
  mm_loglevel = level;
}

/// @brief summarize current heap structure
void mm_check(void)
{
  assert(mm_initialized);

  void *p;
  char *apstr;
  if (get_free_block == ff_get_free_block) apstr = "first fit";
  else if (get_free_block == nf_get_free_block) apstr = "next fit";
  else if (get_free_block == bf_get_free_block) apstr = "best fit";
  else apstr = "invalid";

  LOG(2, "  allocation policy    %s\n", apstr);
  printf("\n----------------------------------------- mm_check ----------------------------------------------\n");
  printf("  ds_heap_start:          %p\n", ds_heap_start);
  printf("  ds_heap_brk:            %p\n", ds_heap_brk);
  printf("  heap_start:             %p\n", heap_start);
  printf("  heap_end:               %p\n", heap_end);
  printf("  allocation policy:      %s\n", apstr);
  printf("  next_block:             %p\n", nf_ptr);   // this will be needed for the next fit policy

  printf("\n");
  p = PREV_PTR(heap_start);
  printf("  initial sentinel:       %p: size: %6lx (%7ld), status: %s\n",
         p, GET_SIZE(p), GET_SIZE(p), GET_STATUS(p) == ALLOC ? "allocated" : "free");
  p = heap_end;
  printf("  end sentinel:           %p: size: %6lx (%7ld), status: %s\n",
         p, GET_SIZE(p), GET_SIZE(p), GET_STATUS(p) == ALLOC ? "allocated" : "free");
  printf("\n");
  printf("  blocks:\n");

  long errors = 0;
  p = heap_start;
  while (p < heap_end) {
    TYPE hdr = GET(p);
    TYPE size = SIZE(hdr);
    TYPE status = STATUS(hdr);
    printf("    %p: size: %6lx (%7ld), status: %s\n", 
           p, size, size, status == ALLOC ? "allocated" : "free");

    void *fp = p + size - TYPE_SIZE;
    TYPE ftr = GET(fp);
    TYPE fsize = SIZE(ftr);
    TYPE fstatus = STATUS(ftr);

    if ((size != fsize) || (status != fstatus)) {
      errors++;
      printf("    --> ERROR: footer at %p with different properties: size: %lx, status: %lx\n", 
             fp, fsize, fstatus);
    }

    p = p + size;
    if (size == 0) {
      printf("    WARNING: size 0 detected, aborting traversal.\n");
      break;
    }
  }

  printf("\n");
  if ((p == heap_end) && (errors == 0)) printf("  Block structure coherent.\n");
  printf("-------------------------------------------------------------------------------------------------\n");
}
