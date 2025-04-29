<<<<<<< Updated upstream
/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  int inc_limit_ret; // new sbrk

  /* TODO: commit the vmaid */
  // rgnode.vmaid
  rgnode.vmaid = vmaid;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    // DEBUG:
    printf("get free area success\n");

    return 0;
  }
  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  else
  {
    // DEBUG:
    //  printf("cannot get free area\n");
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    // printf("Hello");
    // printf("SBRK: %d  END: %d", cur_vma->sbrk, cur_vma->vm_end);

    // @Nhan: Be careful when using aligned size (inc_sz) to extend sbrk.
    // Since inc_sz is rounded up to the nearest page size, sbrk may hit vm_end
    // sooner than expected, even if there’s still technically some space left.
    // This isn't wrong, but it wastes memory — and over time, we might run out of it.
    // So in practice, aligning everything may not be the best idea.

    if (cur_vma->vm_end - cur_vma->sbrk + 1 < size)
    {
      int sizeleft = cur_vma->sbrk + size - 1 - cur_vma->vm_end; // the rg we gonna allocate will have rg_end at sbrk + size - 1
      int inc_sz = PAGING_PAGE_ALIGNSZ(sizeleft);
      int old_sbrk = cur_vma->sbrk;
      // printf("inc_sz in alloc: %d\n", inc_sz);
      struct sc_regs regs;
      regs.a1 = SYSMEM_INC_OP;
      regs.a2 = vmaid;
      regs.a3 = inc_sz; // vm_end will increase by the aligned size of page (256)

      syscall(caller, 17, &regs);
      // alloc_addr = &old_sbrk;
      // if (get_free_vmrg_area(caller, vmaid, size, &rgnode) != 0)
      // {
      //   pthread_mutex_unlock(&mmvm_lock);
      //   return -1;
      // }
      // DEBUG:
      // printf("print pgd in alloc 3: ");
      // print_pgtbl(caller, 0, -1); // In page table

      rgnode = *get_vm_area_node_at_brk(caller, vmaid, size, inc_sz);
      caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
      caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

      *alloc_addr = rgnode.rg_start;

      cur_vma->sbrk = rgnode.rg_end + 1; // sbrk is one byte higher than the highest rg_end ever allocated
      inc_limit_ret = cur_vma->sbrk;

      return 0;
    }

    // printf("addr in alloc 1: ");
    // printf("addr: %08x\n", *alloc_addr);
    rgnode = *get_vm_area_node_at_brk(caller, vmaid, size, size);
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    // printf("addr in alloc 2: ");
    // printf("addr: %08x\n", *alloc_addr);
    cur_vma->sbrk = rgnode.rg_end + 1;
    inc_limit_ret = cur_vma->sbrk;
    // DEBUG:
    // printf("print pgd in alloc 4: ");
    // print_pgtbl(caller, 0, -1); // In page table
  }

  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */

/* @Nhan
 * Enhanced __free() implementation with adjacent free region merging.
 *
 * When a memory region is freed, it may be adjacent to existing free regions
 * in the vm_freerg_list. To reduce fragmentation and maximize available memory,
 * we merge all adjacent regions into one single free region.
 *
 * The function scans through the free region list once (O(n)) and merges any region
 * whose start or end matches the freed region's end or start respectively.
 *
 * Merged regions are removed from the list and their memory is released.
 * The resulting merged region is then inserted back into the free list.
 *
 * This approach avoids costly O(n^2) performance caused by repeatedly restarting
 * the scan for each merge. Only a single pass is needed to complete all merges.
 */

int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  struct vm_rg_struct *symrg = get_symrg_byid(caller->mm, rgid);
  if (!symrg || symrg->rg_start == -1)
    return -1;

  unsigned long free_s = symrg->rg_start;
  unsigned long free_e = symrg->rg_end;
  symrg->rg_start = symrg->rg_end = -1;

  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  if (!vma)
    return -1;

  struct vm_rg_struct *cur = vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  while (cur)
  {
    int need_merge = 0;

    if (free_e == cur->rg_start)
    {
      free_e = cur->rg_end;
      need_merge = 1;
    }
    else if (free_s == cur->rg_end)
    {
      free_s = cur->rg_start;
      need_merge = 1;
    }

    if (need_merge)
    {
      // Remove cur from list
      if (prev == NULL)
        vma->vm_freerg_list = cur->rg_next;
      else
        prev->rg_next = cur->rg_next;

      struct vm_rg_struct *tmp = cur;
      cur = cur->rg_next;
      free(tmp);
      continue;
    }

    prev = cur;
    cur = cur->rg_next;
  }

  struct vm_rg_struct *new_free = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
  new_free->rg_start = free_s;
  new_free->rg_end = free_e;
  new_free->vmaid = vmaid;
  new_free->rg_next = NULL;

  enlist_vm_freerg_list(caller->mm, new_free);

  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  // DEBUG:
  // printf("first print: ");
  // print_pgtbl(proc, 0, -1);

  int addr = 0;
  int freerg_id = -1;    // the region id that has not been in logical address, so also not point to a frame
  int freerg_vmaid = -1; // @TuanAnh: we also need to update freerg_vmaid to use _alloc()
  if (proc->mm->symrgtbl[reg_index].rg_start == -1 &&
      proc->mm->symrgtbl[reg_index].rg_end == -1)
  {
    // printf("use reg_index\n"); DEBUG
    freerg_id = reg_index;
    freerg_vmaid = proc->mm->symrgtbl[reg_index].vmaid;
  }
  else
  {
    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
      if (proc->mm->symrgtbl[i].rg_start == -1 &&
          proc->mm->symrgtbl[i].rg_end == -1)
      {
        freerg_id = i;
        freerg_vmaid = proc->mm->symrgtbl[i].vmaid;
        break;
      }
    }
  }

  if (freerg_id == -1)
    return -1;
  // DEBUG:
  // printf("2 print: ");
  // print_pgtbl(proc, 0, -1); // In page table
  // printf("addr in liballoc 1: %08x\n", addr);
  if (__alloc(proc, 0, freerg_id, size, &addr) == 0)
  {
    // proc->regs[reg_index] = addr;
    proc->regs[freerg_id] = addr;
  }
  // printf("addr in liballoc 2: %08x\n", addr);
  // DEBUG:
  // printf("3 print: ");
  // print_pgtbl(proc, 0, -1); // In page table
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  printf("PID=%d - Region=%d - Address=%08x - Size=%u byte\n", proc->pid, freerg_id, addr, size);
#ifdef PAGETBL_DUMP

  print_pgtbl(proc, 0, -1); // In page table
#endif
  MEMPHY_dump(proc->mram); // In nội dung RAM
  printf("================================================================\n");
#endif
  return 0;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  uint32_t region_id = -1;
  uint32_t region_vmaid = -1; // @TuanAnh: we also need to get region_vmaid to use _free()
  if (reg_index >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  if (proc->mm->symrgtbl[reg_index].rg_start == proc->regs[reg_index])
  {
    region_id = reg_index;
    region_vmaid = proc->mm->symrgtbl[reg_index].vmaid;
  }
  else
  {
    for (size_t i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
      if (proc->mm->symrgtbl[i].rg_start == proc->regs[reg_index] &&
          proc->mm->symrgtbl[i].rg_end > proc->mm->symrgtbl[i].rg_start)
      {
        region_id = i;
        region_vmaid = proc->mm->symrgtbl[i].vmaid;
        break;
      }
    }
  }

  if (region_id == -1)
    return -1;

  int result = __free(proc, 0, region_id);

  if (result == 0)
  {
    proc->regs[region_id] = -1;

#ifdef IODUMP
    printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
    printf("PID=%d - Region=%d (Virtual [%lu - %lu])\n",
           proc->pid,
           region_id,
           proc->mm->symrgtbl[region_id].rg_start,
           proc->mm->symrgtbl[region_id].rg_end);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);
#endif
    MEMPHY_dump(proc->mram);
#endif
  }

  return result;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  // first of all we need to check the page present or not
  uint32_t pte = mm->pgd[pgn];
  if (PAGING_PAGE_PRESENT(pte))
  {
    *fpn = PAGING_FPN(pte);
    enlist_pgn_node(&mm->fifo_pgn, pgn);
    // printf("present\n");
  }
  else
  {
    // it means the page  dont have the physical memory in ram
    //  @Nhan: I consider two cases happen, first the physcical memory had not been allocated yet.
    //  So we need to check if ram have enough space for allocation, we will do it
    //  othrerwise if ram have no space, we will SWAP for swap in and swap out

    int vicpgn, vicfpn, swpfpn;
    int tgtfpn = PAGING_PTE_SWP(pte);
    int freefpn;
    if (MEMPHY_get_freefp(caller->mram, &freefpn) != -1)
    {
      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, freefpn);
      pte_set_fpn(&mm->pgd[pgn], freefpn);
      // pte_set_present(&mm->pgd[pgn]);
      *fpn = freefpn;
      enlist_pgn_node(&mm->fifo_pgn, pgn); // 💥 add to FIFO after allocation
      printf("Have enough space for allocation!");
    }
    // this means when we dont have enough memory for allocation!
    else
    {
      // first of all we need to find where exactly our pages is
      // We know that in the structure of pte when swapped = 1
      // we have the swptype and swpoffset
      // which actualy means swptype is the region, and swpoffset is actual index in that region
      // because the swap ad ram have same struct memphy ....
      if (find_victim_page(caller->mm, &vicpgn) < 0)
      {
        return -1;
      }

      vicfpn = PAGING_FPN(mm->pgd[vicpgn]);
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
      {
        printf("There is no space in swap\n"); // for debuging
        return -1;
      }
      // swap the victim fpn to the swp at the location swpfpn
      __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);
      //@Nhan:
      // after swap we need to modify the pte to let the page table know that the vicfpn is not actualy in ram
      //  but it has been stored in swp
      // about the swptype i could be wrong, the right way is determine exactly what page number of swpfpn;
      // but im too lazy for figuring out this :)).
      pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

      pte_set_fpn(&mm->pgd[pgn], vicfpn);
      *fpn = vicfpn;
      enlist_pgn_node(&mm->fifo_pgn, pgn); // 💥 add to FIFO after swap-in
      // printf("swaped\n");
    }
  }
  // printf("fpn in getpage: %d\n", *fpn);
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  // printf("fpn in getval: %d\n", fpn);
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  // printf("phyaddr in getval: %08x\n", phyaddr);
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  // regs.a3 = *data; // @TuanAnh: we dont need a3 for SYSTEM_IO_READ

  /* SYSCALL 17 sys_memmap */
  syscall(caller, 17, &regs);
  // Update data
  *data = regs.a3;
  // printf("data in getval: %d\n", *data);
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{

  int pgn = PAGING_PGN(addr);   // extract pgn from logical address
  int off = PAGING_OFFST(addr); // extract offset from logical address
  int fpn;

  /* Get the fpn in MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  printf("fpn: %d\n", fpn);
  printf("off: %d\n", off);

  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  printf("phyaddr: %d\n", phyaddr);
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr; // the addr of the exact byte in RAM
  regs.a3 = value;
  syscall(caller, 17, &regs);
  /* SYSCALL 17 sys_memmap */

  // Update data
  // data = (BYTE)

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  if (proc->mm->symrgtbl[source].rg_start == -1 && proc->mm->symrgtbl[source].rg_end == -1)
  {
    // __alloc(proc, 0, destination, proc->mm->symrgtbl[destination].rg_start + offset, &addr);
    printf("SEGV: Attempted read to an invalid address\n");
  }
  else if (proc->mm->symrgtbl[source].rg_start + offset > proc->mm->symrgtbl[source].rg_end)
  {
    printf("SEGV: Attempted read to an invalid address\n");
  }
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  if (val == 0)
  {
    *destination = data;
  }

#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
  printf("PID=%d read region=%d offset=%d value=%d\n", proc->pid, source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  printf("================================================================\n");
  MEMPHY_dump(proc->mram);

#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  // printf("cur_rg start in __write: %08x\n", currg->rg_start);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  // printf("logical addr: %d\n", currg->rg_start + offset);
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  printf("================================================================\n");
  MEMPHY_dump(caller->mram);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register // region id
    uint32_t offset)
{
  int addr;
  if (proc->mm->symrgtbl[destination].rg_start == -1 && proc->mm->symrgtbl[destination].rg_end == -1)
  {
    // __alloc(proc, 0, destination, proc->mm->symrgtbl[destination].rg_start + offset, &addr);
    printf("SEGV: Attempted write to an invalid address\n");
  }
  else if (proc->mm->symrgtbl[destination].rg_start + offset > proc->mm->symrgtbl[destination].rg_end)
  {
    printf("SEGV: Attempted write to an invalid address\n");
  }

  // printf("alloc_addr in write: %08x\n", addr);
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
  printf("PID=%d write region=%d offset=%d value=%d\n", proc->pid, destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  // MEMPHY_dump(proc->mram);
#endif
  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
    else
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
  }

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  // TODO
  // At this we implement LRU algorithm, because due to statictis the LRU have high hit rate comparing to each others
  //@Nhan i using the implement of LRU algorithm to find the victim page
  //  that means "LEAST RECENTY USED"
  //  used the list to obtain the least use by removing the last element of the list, and assing it to retpgn
  struct pgn_t *pg = mm->fifo_pgn;
  if (pg == NULL)
  {
    return -1;
  }
  if (pg->pg_next == NULL)
  {
    *retpgn = pg->pgn;
    free(pg);
    mm->fifo_pgn = NULL;
    return 0;
  }
  while (pg->pg_next->pg_next)
  {
    pg = pg->pg_next;
  }
  *retpgn = pg->pg_next->pgn;
  free(pg->pg_next);
  pg->pg_next = NULL;
  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list; // dia chia dau cua current

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  struct vm_rg_struct *temp = NULL;
  newrg->rg_start = newrg->rg_end = -1;
  /* TODO Traverse on list of free vm region to find a fit space */
  // while (...)
  // freelist-> [0..9] -> [12 ...22]
  // [0->3]
  while (rgit != NULL)
  {
    if (rgit->rg_end - rgit->rg_start + 1 >= size)
    {
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size - 1;

      rgit->rg_start = newrg->rg_end + 1;

      if (rgit->rg_start > rgit->rg_end)
      {
        if (temp == NULL) // firstnode
        {
          struct vm_rg_struct *del = rgit;
          rgit = rgit->rg_next;
          free(del);
        }
        else
        {
          struct vm_rg_struct *del = temp->rg_next;
          temp->rg_next = rgit->rg_next;
          free(del);
        }
      }
      return 0;
    }
    temp = rgit;
    rgit = rgit->rg_next;
  }
  return -1;
}

// #endif
=======
/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "string.h"
#include "mm.h"
#include "syscall.h"
#include "libmem.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
  struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

  if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1;

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  mm->mmap->vm_freerg_list = rg_elmt;

  return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;
  int inc_limit_ret; // new sbrk

  /* TODO: commit the vmaid */
  // rgnode.vmaid
  rgnode.vmaid = vmaid;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;
    if (rgnode.rg_end + 1 == cur_vma->sbrk)
      cur_vma->alloc_point = cur_vma->sbrk;
    *alloc_addr = rgnode.rg_start;
    // DEBUG:
    printf("get free area success\n");

    return 0;
  }
  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  else
  {
    // DEBUG:
    //  printf("cannot get free area\n");
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    // printf("Hello");
    // printf("SBRK: %d  END: %d", cur_vma->sbrk, cur_vma->vm_end);

    // @Nhan: Be careful when using aligned size (inc_sz) to extend sbrk.
    // Since inc_sz is rounded up to the nearest page size, sbrk may hit vm_end
    // sooner than expected, even if there’s still technically some space left.
    // This isn't wrong, but it wastes memory — and over time, we might run out of it.
    // So in practice, aligning everything may not be the best idea.

    //@TuanAnh: if
    if (cur_vma->sbrk != cur_vma->alloc_point)
    {
      if (cur_vma->vm_end - 1 - cur_vma->alloc_point + 1 < size)
      {
        int sizeleft = cur_vma->alloc_point + size - cur_vma->vm_end;
        int inc_sz = PAGING_PAGE_ALIGNSZ(sizeleft);
        struct sc_regs regs;
        regs.a1 = SYSMEM_INC_OP;
        regs.a2 = vmaid;
        regs.a3 = inc_sz; // vm_end will increase by the aligned size of page (256), vm_end + inc_sz

        if (syscall(caller, 17, &regs) != 0)
        {
          printf("Syscall failed in __alloc\n");
          return -1;
        }
      }

      caller->mm->symrgtbl[rgid].rg_start = cur_vma->alloc_point;
      caller->mm->symrgtbl[rgid].rg_end = cur_vma->alloc_point + size - 1;
      inc_limit_ret = caller->mm->symrgtbl[rgid].rg_start;
      *alloc_addr = inc_limit_ret;
      cur_vma->alloc_point = caller->mm->symrgtbl[rgid].rg_end + 1; // increase alloc_point because we have just alloc new rg at the top
      if (cur_vma->alloc_point > cur_vma->sbrk)
        cur_vma->sbrk = cur_vma->alloc_point;
      return 0;
    }
    else if (cur_vma->vm_end - cur_vma->sbrk < size) // the new rg should be from [sbrk -> vm_end - 1], because vm_end is 1 byte higher
    {
      int sizeleft = cur_vma->sbrk + size - cur_vma->vm_end; // the rg we gonna allocate will have rg_end at sbrk + size - 1
      int inc_sz = PAGING_PAGE_ALIGNSZ(sizeleft);
      // int old_sbrk = cur_vma->sbrk;
      // printf("inc_sz in alloc: %d\n", inc_sz);
      struct sc_regs regs;
      regs.a1 = SYSMEM_INC_OP;
      regs.a2 = vmaid;
      regs.a3 = inc_sz; // vm_end will increase by the aligned size of page (256), vm_end + inc_sz

      if (syscall(caller, 17, &regs) != 0)
      {
        printf("Syscall failed in __alloc\n");
        return -1;
      }

      rgnode = *get_vm_area_node_at_brk(caller, vmaid, size, inc_sz);
      caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
      caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end; // rg_end - rg_start + 1 = size

      cur_vma->sbrk = caller->mm->symrgtbl[rgid].rg_end + 1; // sbrk is one byte higher than the highest rg_end ever allocated
      cur_vma->alloc_point = cur_vma->sbrk;
      inc_limit_ret = caller->mm->symrgtbl[rgid].rg_start;
      *alloc_addr = inc_limit_ret;
      return 0;
    }

    // @TuanAnh: if [sbrk, vm_end - 1] has enough space, so we just grab it
    rgnode = *get_vm_area_node_at_brk(caller, vmaid, size, size);
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    cur_vma->sbrk = caller->mm->symrgtbl[rgid].rg_end + 1;
    cur_vma->alloc_point = cur_vma->sbrk;
    inc_limit_ret = caller->mm->symrgtbl[rgid].rg_start;
    *alloc_addr = inc_limit_ret;
  }

  return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */

/* @Nhan
 * Enhanced __free() implementation with adjacent free region merging.
 *
 * When a memory region is freed, it may be adjacent to existing free regions
 * in the vm_freerg_list. To reduce fragmentation and maximize available memory,
 * we merge all adjacent regions into one single free region.
 *
 * The function scans through the free region list once (O(n)) and merges any region
 * whose start or end matches the freed region's end or start respectively.
 *
 * Merged regions are removed from the list and their memory is released.
 * The resulting merged region is then inserted back into the free list.
 *
 * This approach avoids costly O(n^2) performance caused by repeatedly restarting
 * the scan for each merge. Only a single pass is needed to complete all merges.
 */

int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  struct vm_rg_struct *symrg = get_symrg_byid(caller->mm, rgid);
  if (!symrg || symrg->rg_start == -1)
    return -1;

  unsigned long free_s = symrg->rg_start;
  unsigned long free_e = symrg->rg_end;
  symrg->rg_start = symrg->rg_end = -1;

  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);
  if (!vma)
    return -1;

  if (free_e == vma->alloc_point - 1)
  {
    vma->alloc_point = free_s;
  }

  struct vm_rg_struct *cur = vma->vm_freerg_list;
  struct vm_rg_struct *prev = NULL;

  while (cur)
  {
    int need_merge = 0;

    if (free_e == cur->rg_start)
    {
      free_e = cur->rg_end;
      need_merge = 1;
    }
    else if (free_s == cur->rg_end)
    {
      free_s = cur->rg_start;
      need_merge = 1;
    }

    if (need_merge)
    {
      // Remove cur from list
      if (prev == NULL)
        vma->vm_freerg_list = cur->rg_next;
      else
        prev->rg_next = cur->rg_next;

      struct vm_rg_struct *tmp = cur;
      cur = cur->rg_next;
      free(tmp);
      continue;
    }

    prev = cur;
    cur = cur->rg_next;
  }

  struct vm_rg_struct *new_free = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));
  new_free->rg_start = free_s;
  new_free->rg_end = free_e;
  new_free->vmaid = vmaid;
  new_free->rg_next = NULL;

  enlist_vm_freerg_list(caller->mm, new_free);
  printf("Free Region List:\n");
  struct vm_rg_struct *curr = caller->mm->mmap->vm_freerg_list;
  while (curr)
  {
    printf("Rg_start %d - Rg_end%d", curr->rg_start, curr->rg_end);
    curr = curr->rg_next;
  }
  printf("\n");
  return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{

  int addr = 0;
  int freerg_id = -1; // the region id that has not been in logical address, so also not point to a frame
  if (proc->mm->symrgtbl[reg_index].rg_start == -1 &&
      proc->mm->symrgtbl[reg_index].rg_end == -1)
  {
    freerg_id = reg_index;
  }
  else
  {
    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
      if (proc->mm->symrgtbl[i].rg_start == -1 &&
          proc->mm->symrgtbl[i].rg_end == -1)
      {
        freerg_id = i;
        break;
      }
    }
  }

  if (freerg_id == -1)
    return -1;
  if (__alloc(proc, 0, freerg_id, size, &addr) == 0)
  {
    proc->regs[reg_index] = addr;
  }
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
  printf("PID=%d - Region=%d - Address=%08x - Size=%u byte\n", proc->pid, freerg_id, addr, size);
#ifdef PAGETBL_DUMP

  print_pgtbl(proc, 0, -1); // In page table
#endif
  MEMPHY_dump(proc->mram);
  printf("================================================================\n");
#endif
  return 0;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int libfree(struct pcb_t *proc, uint32_t reg_index)
{
  uint32_t region_id = -1;

  if (reg_index >= PAGING_MAX_SYMTBL_SZ)
    return -1;

  if (proc->mm->symrgtbl[reg_index].rg_start == proc->regs[reg_index])
  {
    region_id = reg_index;
  }
  else
  {
    for (size_t i = 0; i < PAGING_MAX_SYMTBL_SZ; i++)
    {
      if (proc->mm->symrgtbl[i].rg_start == proc->regs[reg_index] &&
          proc->mm->symrgtbl[i].rg_end > proc->mm->symrgtbl[i].rg_start)
      {
        region_id = i;
        break;
      }
    }
  }

  if (region_id == -1)
    return -1;

  int result = __free(proc, 0, region_id);

  if (result == 0)
  {
    // proc->regs[region_id] = -1;
    proc->regs[reg_index] = -1;

#ifdef IODUMP
    printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
    printf("PID=%d - Region=%d\n",
           proc->pid,
           region_id);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1);

#endif
    MEMPHY_dump(proc->mram);
#endif
  }

  return result;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  // first of all we need to check the page present or not
  uint32_t pte = mm->pgd[pgn];
  if (PAGING_PAGE_PRESENT(pte))
  {
    *fpn = PAGING_FPN(pte);
    enlist_pgn_node(&mm->fifo_pgn, pgn);
    // printf("present\n");
  }
  else
  {
    // it means the page  dont have the physical memory in ram
    //  @Nhan: I consider two cases happen, first the physcical memory had not been allocated yet.
    //  So we need to check if ram have enough space for allocation, we will do it
    //  othrerwise if ram have no space, we will SWAP for swap in and swap out

    int vicpgn, vicfpn, swpfpn;
    int tgtfpn = PAGING_PTE_SWP(pte);
    int freefpn;

    if (MEMPHY_get_freefp(caller->mram, &freefpn) != -1)
    {
      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, freefpn);
      pte_set_fpn(&mm->pgd[pgn], freefpn);
      // pte_set_present(&mm->pgd[pgn]);
      *fpn = freefpn;
      enlist_pgn_node(&mm->fifo_pgn, pgn); //  add to FIFO after allocation
      printf("Have enough space for allocation!");
    }
    // this means when we dont have enough memory for allocation!
    else
    {
      // first of all we need to find where exactly our pages is
      // We know that in the structure of pte when swapped = 1
      // we have the swptype and swpoffset
      // which actualy means swptype is the region, and swpoffset is actual index in that region
      // because the swap ad ram have same struct memphy ....
      if (find_victim_page(caller->mm, &vicpgn) < 0)
      {
        return -1;
      }

      vicfpn = PAGING_FPN(mm->pgd[vicpgn]);
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
      {
        printf("There is no space in swap\n"); // for debuging
        return -1;
      }
      // swap the victim fpn to the swp at the location swpfpn
      __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);
      //@Nhan:
      // after swap we need to modify the pte to let the page table know that the vicfpn is not actualy in ram
      //  but it has been stored in swp
      // about the swptype i could be wrong, the right way is determine exactly what page number of swpfpn;
      // but im too lazy for figuring out this :)).
      pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn);

      pte_set_fpn(&mm->pgd[pgn], vicfpn);
      *fpn = vicfpn;
      enlist_pgn_node(&mm->fifo_pgn, pgn); // 💥 add to FIFO after swap-in
      // printf("swaped\n");
    }
  }
  // printf("fpn in getpage: %d\n", *fpn);
  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_read(caller->mram, phyaddr, data);
   *  MEMPHY READ
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
   */
  // printf("fpn in getval: %d\n", fpn);
  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  // printf("phyaddr in getval: %08x\n", phyaddr);
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_READ;
  regs.a2 = phyaddr;
  // regs.a3 = *data; // @TuanAnh: we dont need a3 for SYSTEM_IO_READ

  /* SYSCALL 17 sys_memmap */
  // syscall(caller, 17, &regs);
  if (syscall(caller, 17, &regs) < 0)
  {
    printf("Syscall read failed");
    return -1;
  }
  // Update data
  *data = regs.a3;
  // printf("data in getval: %d\n", *data);
  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{

  int pgn = PAGING_PGN(addr);   // extract pgn from logical address
  int off = PAGING_OFFST(addr); // extract offset from logical address
  int fpn;

  /* Get the fpn in MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  /* TODO
   *  MEMPHY_write(caller->mram, phyaddr, value);
   *  MEMPHY WRITE
   *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
   */
  printf("fpn: %d\n", fpn);
  printf("off: %d\n", off);

  int phyaddr = (fpn * PAGING_PAGESZ) + off;
  printf("phyaddr: %d\n", phyaddr);
  struct sc_regs regs;
  regs.a1 = SYSMEM_IO_WRITE;
  regs.a2 = phyaddr; // the addr of the exact byte in RAM
  regs.a3 = value;
  if (syscall(caller, 17, &regs) < 0)
  {
    printf("Syscall write failed");
    return -1;
  }
  /* SYSCALL 17 sys_memmap */

  // Update data
  // data = (BYTE)

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*libread - PAGING-based read a region memory */
int libread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t *destination)
{
  BYTE data;
  if (proc->mm->symrgtbl[source].rg_start == -1 && proc->mm->symrgtbl[source].rg_end == -1)
  {
    // __alloc(proc, 0, destination, proc->mm->symrgtbl[destination].rg_start + offset, &addr);
    printf("SEGV: Attempted read to an invalid address\n");
  }
  else if (proc->mm->symrgtbl[source].rg_start + offset > proc->mm->symrgtbl[source].rg_end)
  {
    printf("SEGV: Attempted read to an invalid address\n");
  }
  int val = __read(proc, 0, source, offset, &data);

  /* TODO update result of reading action*/
  if (val == 0)
  {
    *destination = data;
  }

#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
  printf("PID=%d read region=%d offset=%d value=%d\n", proc->pid, source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  printf("================================================================\n");
  MEMPHY_dump(proc->mram);

#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
  // printf("cur_rg start in __write: %08x\n", currg->rg_start);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  // printf("logical addr: %d\n", currg->rg_start + offset);
  pg_setval(caller->mm, currg->rg_start + offset, value, caller);
  printf("================================================================\n");
  MEMPHY_dump(caller->mram);
  return 0;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register // region id
    uint32_t offset)
{
  // int addr;
  if (proc->mm->symrgtbl[destination].rg_start == -1 && proc->mm->symrgtbl[destination].rg_end == -1)
  {
    // __alloc(proc, 0, destination, proc->mm->symrgtbl[destination].rg_start + offset, &addr);
    printf("SEGV: Attempted write to an invalid address\n");
  }
  else if (proc->mm->symrgtbl[destination].rg_start + offset > proc->mm->symrgtbl[destination].rg_end)
  {
    printf("SEGV: Attempted write to an invalid address\n");
  }

  // printf("alloc_addr in write: %08x\n", addr);
#ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
  printf("PID=%d write region=%d offset=%d value=%d\n", proc->pid, destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1);

  // print max TBL
#endif
  // MEMPHY_dump(proc->mram);
#endif
  return __write(proc, 0, destination, offset, data);
}

int mmap(struct pcb_t *caller, int regionid)
{

  struct vm_rg_struct *rg = get_symrg_byid(caller->mm, regionid);
  // int numpage = (rg->rg_end - rg->rg_start + 1) / PAGING_PAGESZ;
  // printf("numpage: %d\n", numpage);
  int start = rg->rg_start;
  int end = rg->rg_end;

  for (int pgit = start; pgit < end; pgit += PAGING_PAGESZ)
  {

    int pgn = PAGING_PGN(pgit);
    int offset = PAGING_OFFST(pgit);
    int fpn;

    if (pg_getpage(caller->mm, pgn, &fpn, caller) != 0)
    {
      printf("Cannot Get Physical Address\n");
      continue;
    }
    int phyaddr = (fpn * PAGING_PAGESZ) + offset;

    printf("Virtual Addr: %08x -> Physical Addr: %08x\n", pgit * PAGING_PAGESZ, phyaddr); // print the page index in the pgd, and its corresponding fpn
  }
  return 0;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
    else
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
  }

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  // TODO
  // At this we implement LRU algorithm, because due to statictis the LRU have high hit rate comparing to each others
  //@Nhan i using the implement of LRU algorithm to find the victim page
  //  that means "LEAST RECENTY USED"
  //  used the list to obtain the least use by removing the last element of the list, and assing it to retpgn
  struct pgn_t *pg = mm->fifo_pgn;
  if (pg == NULL)
  {
    return -1;
  }
  if (pg->pg_next == NULL)
  {
    *retpgn = pg->pgn;
    free(pg);
    mm->fifo_pgn = NULL;
    return 0;
  }
  while (pg->pg_next->pg_next)
  {
    pg = pg->pg_next;
  }
  *retpgn = pg->pg_next->pgn;
  free(pg->pg_next);
  pg->pg_next = NULL;
  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (!cur_vma || cur_vma->vm_freerg_list == NULL)
    return -1;

  // Khởi tạo các con trỏ để duyệt và lưu best-fit
  struct vm_rg_struct *prev = NULL, *curr = cur_vma->vm_freerg_list;
  struct vm_rg_struct *best = NULL, *best_prev = NULL;
  int best_waste = 999999;

  while (curr)
  {
    int length = curr->rg_end - curr->rg_start + 1;
    if (length >= size)
    {
      int waste = length - size;
      if (waste < best_waste)
      {
        best_waste = waste;
        best = curr;
        best_prev = prev;
      }
    }
    prev = curr;
    curr = curr->rg_next;
  }

  if (!best)
    return -1;

  newrg->rg_start = best->rg_start;
  newrg->rg_end = best->rg_start + size - 1;

  best->rg_start = newrg->rg_end + 1;

  if (best->rg_start > best->rg_end)
  {
    if (best_prev)
      best_prev->rg_next = best->rg_next;
    else
      cur_vma->vm_freerg_list = best->rg_next;
    free(best);
  }

  return 0;
}

// #endif
>>>>>>> Stashed changes
