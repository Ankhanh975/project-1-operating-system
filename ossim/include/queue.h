/*
 * queue.h: a FIFO of PCB pointers.
 *
 * Used for ready queues, semaphore waiting lists and MLFQ levels. You can
 * also take an element out of the middle, which SJF, SRTF and priority need.
 */
#ifndef OSSIM_QUEUE_H
#define OSSIM_QUEUE_H

#include "process.h"

typedef struct queue {
    pcb_t *items[MAX_PROCS];
    int    count;
} queue_t;

void   q_init(queue_t *q);
bool   q_empty(const queue_t *q);
int    q_size(const queue_t *q);
void   q_push(queue_t *q, pcb_t *p);        /* add at the tail */
pcb_t *q_pop(queue_t *q);                   /* remove the head and return it */
pcb_t *q_peek(const queue_t *q);            /* the head, not removed */
pcb_t *q_at(const queue_t *q, int i);       /* element i, 0 is the head */
bool   q_remove(queue_t *q, pcb_t *p);      /* remove p from wherever it is */
/* Remove and return the element with the smallest key(p). On a tie, the
 * one nearer the head wins. */
pcb_t *q_pop_min(queue_t *q, int (*key)(const pcb_t *));
/* The smallest key, without removing anything. INT_MAX if q is empty. */
int    q_min_key(const queue_t *q, int (*key)(const pcb_t *));

#endif
