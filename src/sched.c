
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
// struct pcb_t *get_mlq_proc(void)
// {
// 	/*TODO: get a process from PRIORITY [ready_queue].
// 	 * Remember to use lock to protect the queue.
// 	 * */
// 	pthread_mutex_lock(&queue_lock);
// 	struct pcb_t *proc = NULL;
// 	int start_prio = current_prio;
// 	int isFinished = 0;

// 	while (!isFinished)
// 	{
// 		if (!empty(&mlq_ready_queue[current_prio]) && curr_slot < slot[current_prio])
// 		{
// 			proc = dequeue(&mlq_ready_queue[current_prio]);
// 			if (proc != NULL)
// 			{
// 				curr_slot++;
// 				if (curr_slot >= slot[current_prio])
// 				{
// 					curr_slot = 0;
// 					current_prio = (current_prio + 1) % MAX_PRIO;
// 				}
// 				break;
// 			}
// 		}

// 		current_prio = (current_prio + 1) % MAX_PRIO;
// 		curr_slot = 0;
// 		if (current_prio == start_prio)
// 		{
// 			isFinished = 1;
// 		}
// 	}
// 	pthread_mutex_unlock(&queue_lock);

// 	return proc;
// }

struct pcb_t *get_mlq_proc(void)
{
	struct pcb_t *proc = NULL;

	// Acquire lock to protect shared ready queues and slot counters
	pthread_mutex_lock(&queue_lock);

	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		if (slot[prio] > 0 && !empty(&mlq_ready_queue[prio]))
		{
			proc = dequeue(&mlq_ready_queue[prio]);
			if (proc)
			{
				// Consume one time‑slice from this priority level
				slot[prio]--;
			}
			// Release lock and return the selected process
			pthread_mutex_unlock(&queue_lock);
			return proc;
		}
	}

	// 2) No eligible queue found: reset all slot counters for a new period
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		slot[prio] = MAX_PRIO - prio;
	}

	// 3) Second pass: after reset, pick the first non‑empty queue
	for (int prio = 0; prio < MAX_PRIO; prio++)
	{
		if (slot[prio] > 0 && !empty(&mlq_ready_queue[prio]))
		{
			proc = dequeue(&mlq_ready_queue[prio]);
			if (proc)
			{
				slot[prio]--;
			}
			break;
		}
	}

	// Release lock and return (may be NULL if no process is ready)
	pthread_mutex_unlock(&queue_lock);
	return proc;
}
void put_mlq_proc(struct pcb_t *proc)
{
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
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
	return get_mlq_proc();
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
