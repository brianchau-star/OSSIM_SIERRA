<<<<<<< Updated upstream
/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"

#define SYSMEM_MAP_OP 1
#define SYSMEM_INC_OP 2
#define SYSMEM_SWP_OP 3
#define SYSMEM_IO_READ 4
#define SYSMEM_IO_WRITE 5

extern struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid);
int inc_vma_limit(struct pcb_t *, int, int);
int __mm_swap_page(struct pcb_t *, int, int);
int liballoc(struct pcb_t *, uint32_t, uint32_t);
int libfree(struct pcb_t *, uint32_t);
int libread(struct pcb_t *, uint32_t, uint32_t, uint32_t *);
int libwrite(struct pcb_t *, BYTE, uint32_t, uint32_t);
int free_pcb_memph(struct pcb_t *caller);
=======
/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"

#define SYSMEM_MAP_OP 1
#define SYSMEM_INC_OP 2
#define SYSMEM_SWP_OP 3
#define SYSMEM_IO_READ 4
#define SYSMEM_IO_WRITE 5
#define SYSMEM_IO_DUMP 6


extern struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid);
int inc_vma_limit(struct pcb_t *, int, int);
int __mm_swap_page(struct pcb_t *, int, int);
int liballoc(struct pcb_t *, uint32_t, uint32_t);
int libfree(struct pcb_t *, uint32_t);
int libread(struct pcb_t *, uint32_t, uint32_t, uint32_t *);
int libwrite(struct pcb_t *, BYTE, uint32_t, uint32_t);
int free_pcb_memph(struct pcb_t *caller);
int mmap(struct pcb_t *caller, int regionid);
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid);
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller);
>>>>>>> Stashed changes
