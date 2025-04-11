#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t *q)
{
        if (q == NULL)
                return 1;
        return (q->size == 0);
}

void enqueue(struct queue_t *q, struct pcb_t *proc)
{
        /* TODO: put a new process to queue [q] */
        if (q == NULL || proc == NULL || q->size >= MAX_QUEUE_SIZE)
        {
                return;
        }
        q->proc[q->size] = proc;
        q->size++;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        if (q == NULL || q->size == 0)
                return NULL;

#ifdef MLQ_SCHED
        struct pcb_t *proc = q->proc[0];
        for (int i = 1; i < q->size; i++)
                q->proc[i - 1] = q->proc[i];
        q->size--;
        return proc;
#else
        int best = 0;
        for (int i = 1; i < q->size; i++)
        {
                if (q->proc[i]->prio < q->proc[best]->prio)
                        best = i;
        }
        struct pcb_t *proc = q->proc[best];
        for (int i = best + 1; i < q->size; i++)
                q->proc[i - 1] = q->proc[i];
        q->size--;
        return proc;
#endif
}
