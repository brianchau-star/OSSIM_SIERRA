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
        int index = q->size - 1;
        while (index >= 0 && proc->prio < q->proc[index]->prio)
        {
                q->proc[index + 1] = q->proc[index];
                index--;
        }
        q->proc[index + 1] = proc;
        q->size += 1;
}

struct pcb_t *dequeue(struct queue_t *q)
{
        /* TODO: return a pcb whose prioprity is the highest
         * in the queue [q] and remember to remove it from q
         * */

        if (q == NULL || q->size == 0)
        {
                return NULL;
        }
        struct pcb_t *proc = q->proc[0];

        for (int index = 1; index < q->size; index++)
        {
                q->proc[index - 1] = q->proc[index];
        }
        q->size -= 1;

        return proc;
}
