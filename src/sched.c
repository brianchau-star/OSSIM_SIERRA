
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

static struct queue_t running_list;
#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int slot[MAX_PRIO];
static int current_prio = 0;
static int curr_slot = 0;
#endif

int queue_empty(void)
{
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if (!empty(&mlq_ready_queue[prio]))
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void)
{
#ifdef MLQ_SCHED
	int i;

	for (i = 0; i < MAX_PRIO; i++)
	{
		mlq_ready_queue[i].size = 0;
		slot[i] = MAX_PRIO - i;
	}
	current_prio = 0;
	curr_slot = 0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}
struct pcb_t *find_running_proc(uint32_t target_pid)
{
	struct pcb_t *result = NULL;

	pthread_mutex_lock(&queue_lock);
	for (int i = 0; i < running_list.size; i++)
	{
		struct pcb_t *p = running_list.proc[i];
		if (p && p->pid == target_pid)
		{
			result = p;
			break;
		}
	}
	pthread_mutex_unlock(&queue_lock);

	return result;
}
#ifdef MLQ_SCHED
/*
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
/**
 * get_mlq_proc - (Flawed) MLQ selection starting only from current_prio
 *
 * PROBLEM:
 *   This version only ever scans forward from the last used priority (current_prio)
 *   and never restarts at the highest priority (0) on each dispatch. As a result:
 *     - Newly arrived or waiting processes in higher‑priority queues (with prio < current_prio)
 *       will be skipped until the scheduler wraps all the way around in a future period.
 *     - This violates the MLQ requirement to always consider the highest priority queue first
 *       on each scheduling decision.
 *
 * CORRECT BEHAVIOR:
 *   On each call, the scheduler should scan from prio = 0 up to MAX_PRIO−1,
 *   pick the first non‑empty queue whose slot allotment isn’t exhausted,
 *   then dequeue from it. Only after selecting a queue should it advance current_prio
 *   (and reset slice count) when that queue’s slots run out.
 *
 * TODO:
 *   - Change the scan loop to always start at prio = 0, not at current_prio.
 *   - After dequeueing, update current_prio and curr_slot appropriately.
 */

struct pcb_t *get_mlq_proc(void)
{
	struct pcb_t *proc = NULL;

	pthread_mutex_lock(&queue_lock);

	// 1. First pass: Try to get process with slot available
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		while (slot[prio] > 0 && !empty(&mlq_ready_queue[prio]))
		{
			proc = dequeue(&mlq_ready_queue[prio]);

			if (!proc)
				continue;

			if (proc->pc == proc->code->size)
			{
				printf("[MLQ] Skipping killed process PID %d\n", proc->pid);
				free(proc);
				continue;
			}

			slot[prio]--;
			pthread_mutex_unlock(&queue_lock);
			return proc;
		}
	}

	// 2. Reset slot counters
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		slot[prio] = MAX_PRIO - prio;
	}

	// 3. Second pass: Retry after reset
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		while (slot[prio] > 0 && !empty(&mlq_ready_queue[prio]))
		{
			proc = dequeue(&mlq_ready_queue[prio]);

			if (!proc)
				continue;

			if (proc->pc == proc->code->size)
			{
				printf("[MLQ] Skipping killed process PID %d\n", proc->pid);
				free(proc);
				continue;
			}

			slot[prio]--;
			pthread_mutex_unlock(&queue_lock);
			return proc;
		}
	}

	pthread_mutex_unlock(&queue_lock);
	return NULL;
}

void put_mlq_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	if (proc)
	{
		remove_from_queue(&running_list, proc);
		enqueue(&mlq_ready_queue[proc->prio], proc);
	}
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);			 // lock the queue is adding the new proc
	enqueue(&mlq_ready_queue[proc->prio], proc); // add the new proc to the queue
	pthread_mutex_unlock(&queue_lock);			 // unlock this mutex
}

struct pcb_t *get_proc(void)
{
	struct pcb_t *proc = get_mlq_proc();
	if (proc)
	{
		pthread_mutex_lock(&queue_lock);
		enqueue(&running_list, proc);
		pthread_mutex_unlock(&queue_lock);
	}

	return proc;
}

void put_proc(struct pcb_t *proc)
{
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = &running_list;

	/* TODO: put running proc to running_list */

	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t *proc)
{
	proc->ready_queue = &ready_queue;
	proc->mlq_ready_queue = mlq_ready_queue;
	proc->running_list = &running_list;

	/* TODO: put running proc to running_list */

	return add_mlq_proc(proc);
}
#else
struct pcb_t *get_proc(void)
{
	struct pcb_t *proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	return proc;
}

void put_proc(struct pcb_t *proc)
{
	proc->ready_queue = &ready_queue;
	proc->running_list = &running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t *proc)
{
	proc->ready_queue = &ready_queue;
	proc->running_list = &running_list;

	/* TODO: put running proc to running_list */

	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}
#endif
