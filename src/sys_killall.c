/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */
#include "common.h"
#include "syscall.h"
#include "stdio.h"
#include "libmem.h"
#include "queue.h"
#include <string.h>
#include <stdlib.h>

// The code logic for syscall is good but actualy it contains bug in the fuction lib_read
// @Nhan mention @Tuan Anh need to check logic again in the fucion libread
// cuz the func libread is calling the func pg_getval
// so you can start from pg_getval
// im done my fucking work for this fucking assingment for 5 days
// gud luck:)))
int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
{
    char proc_name[100];
    uint32_t data;

    // hardcode for demo only
    uint32_t region_id = regs->a1;
    // uint32_t region_base = caller->mm->symrgtbl[region_id].rg_start;
    /* TODO: Get name of the target proc */
    // proc_name = libread..

    int i = 0;
    data = 0;
    while (data != -1)
    {
        libread(caller, region_id, i, &data);
        proc_name[i] = data;
        if (data == -1)
            proc_name[i] = '\0';
        i++;
    }
    printf("The procname retrieved from memregionid %d is \"%s\"\n", region_id, proc_name);

    /* ------------------------------------------------------------------
     * 1) Remove from running_list
     * ------------------------------------------------------------------*/
    if (caller->running_list != NULL)
    {
        int idx = 0;
        while (idx < caller->running_list->size)
        {

            printf("Size neeeeee %d \n", caller->running_list->size);
            struct pcb_t *p = caller->running_list->proc[idx];
            if (p && strcmp(p->path + 11, proc_name) == 0)
            {
                if (p == caller)
                {
                    printf("Killall: Caller (PID %u) marks itself as finished.\n", p->pid);
                    p->pc = p->code->size;
                    p->is_killed = 1;
                    free_pcb_memph(p);
                }
                else
                {
                    printf("Killall: Freed process PID %u: %s\n", p->pid, p->path);
                    p->pc = p->code->size;
                    p->is_killed = 1;
                    // libfree(p, region_id);
                    free_pcb_memph(p);
                }
                for (int j = idx; j < caller->running_list->size - 1; j++)
                {
                    caller->running_list->proc[j] = caller->running_list->proc[j + 1];
                }
                caller->running_list->size--;
            }
            else
            {
                idx++;
            }
        }
    }

#ifdef MLQ_SCHED
    /* ------------------------------------------------------------------
     * 2) Remove from MLQ ready queues
     * ------------------------------------------------------------------*/
    if (caller->mlq_ready_queue != NULL)
    {
        for (int prio = 0; prio < MAX_PRIO; prio++)
        {
            struct queue_t *q = &caller->mlq_ready_queue[prio];

            int idx = 0;
            while (idx < q->size)
            {
                struct pcb_t *p = q->proc[idx];
                if (p && strcmp(p->path + 11, proc_name) == 0)
                {
                    if (p == caller)
                    {
                        printf("Killall: Caller (PID %u) marks itself as finished.\n", p->pid);
                        p->is_killed = 1;
                        p->pc = p->code->size;
                    }
                    else
                    {
                        printf("Killall: Freed process PID %u: %s\n", p->pid, p->path);
                        printf("path %s  pid %d is killed  %d \n ", p->path, p->pid, p->is_killed);
                        p->is_killed = 1;
                        p->pc = p->code->size;
                        free_pcb_memph(p);
                    }
                    /* Shift queue elements left */
                    for (int j = idx; j < q->size - 1; j++)
                    {
                        q->proc[j] = q->proc[j + 1];
                    }
                    q->size--;
                }
                else
                {
                    idx++;
                }
            }
        }
    }
#endif

    /* If you also have a single-level ready_queue,
     * you can do a similar loop over caller->ready_queue here. */

    return 0;
}
