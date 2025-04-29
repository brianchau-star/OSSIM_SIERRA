<<<<<<< Updated upstream
#ifndef OSMM_H
#define OSMM_H


#define MM_PAGING
#define PAGING_MAX_MMSWP 4 /* max number of supported swapped space */
#define PAGING_MAX_SYMTBL_SZ 30

typedef char BYTE;
typedef uint32_t addr_t;
//typedef unsigned int uint32_t;

struct pgn_t{
   int pgn;
   struct pgn_t *pg_next; 
};

/*
 *  Memory region struct
 */
struct vm_rg_struct {
   unsigned long rg_start;
   unsigned long rg_end;
   unsigned long vmaid;
   struct vm_rg_struct *rg_next;
};

/*
 *  Memory area struct
 */
struct vm_area_struct {
   unsigned long vm_id;
   unsigned long vm_start;
   unsigned long vm_end;

   unsigned long sbrk;
/*
 * Derived field
 * unsigned long vm_limit = vm_end - vm_start
 */
   struct mm_struct *vm_mm;
   struct vm_rg_struct *vm_freerg_list;
   struct vm_area_struct *vm_next;
};

/* 
 * Memory management struct
 */
struct mm_struct {
   uint32_t *pgd; // SELF NOTE: page table

   struct vm_area_struct *mmap; // SELF NOTE: a pointer to the first vm_area of a linkedlist of vm_area

   /* Currently we support a fixed number of symbol */
   struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ]; // lsit of region that has been allocated (point to a frame that has data)

   /* list of free page */
   struct pgn_t *fifo_pgn;
};

/*
 * FRAME/MEM PHY struct
 */
struct framephy_struct { 
   int fpn;
   struct framephy_struct *fp_next;

   /* Resereed for tracking allocated framed */
   struct mm_struct* owner;
};

struct memphy_struct {
   /* Basic field of data and size */
   BYTE *storage;
   int maxsz;
   
   /* Sequential device fields */ 
   int rdmflg;
   int cursor;

   /* Management structure */
   struct framephy_struct *free_fp_list;
   struct framephy_struct *used_fp_list;
};

#endif
=======
#ifndef OSMM_H
#define OSMM_H

#define MM_PAGING
#define PAGING_MAX_MMSWP 4 /* max number of supported swapped space */
#define PAGING_MAX_SYMTBL_SZ 30
// #include <pthread.h>
typedef char BYTE;
typedef uint32_t addr_t;
// typedef unsigned int uint32_t;

struct pgn_t
{
   int pgn;
   struct pgn_t *pg_next;
};

/*
 *  Memory region struct
 */
struct vm_rg_struct
{
   unsigned long rg_start;
   unsigned long rg_end;
   unsigned long vmaid;
   struct vm_rg_struct *rg_next;
};

/*
 *  Memory area struct
 */
struct vm_area_struct
{
   unsigned long vm_id;
   unsigned long vm_start;
   unsigned long vm_end;

   unsigned long sbrk;
   unsigned long alloc_point;
   /*
    * Derived field
    * unsigned long vm_limit = vm_end - vm_start
    */
   struct mm_struct *vm_mm;
   struct vm_rg_struct *vm_freerg_list;
   struct vm_area_struct *vm_next;
};

/*
 * Memory management struct
 */
struct mm_struct
{
   uint32_t *pgd; // SELF NOTE: page table

   struct vm_area_struct *mmap; // SELF NOTE: a pointer to the first vm_area of a linkedlist of vm_area

   /* Currently we support a fixed number of symbol */
   struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ]; // lsit of region that has been allocated (point to a frame that has data)

   /* list of free page */
   struct pgn_t *fifo_pgn;
};

/*
 * FRAME/MEM PHY struct
 */
struct framephy_struct
{
   int fpn;
   struct framephy_struct *fp_next;

   /* Resereed for tracking allocated framed */
   struct mm_struct *owner;
};

struct memphy_struct
{
   /* Basic field of data and size */
   BYTE *storage;
   int maxsz;

   /* Sequential device fields */
   int rdmflg;
   int cursor;

   /* Management structure */
   struct framephy_struct *free_fp_list;
   struct framephy_struct *used_fp_list;

   //@Nhan
   // During analysis of the CPU routine and the OS memory management,
   // a critical issue was identified: race conditions.
   // Since the memory system acts as a critical section,
   // multiple threads (CPUs) may concurrently access and modify
   // shared structures such as `free_fp_list`, `used_fp_list`, and `storage`.
   // Without proper synchronization, concurrent modifications
   // can lead to data corruption and unpredictable behavior.

   // We add two mutexes: one for accessing storage (read/write) and one for managing the free frame list.

   // pthread_mutex_t free_lock;
   // pthread_mutex_t access_lock;
};

#endif
>>>>>>> Stashed changes
