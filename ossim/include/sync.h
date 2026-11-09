/*
 * sync.h: semaphores, resources and deadlock detection.
 *
 * Semaphores are as in Chapter 6, without busy waiting. wait() on a
 * semaphore with value 0 puts the PCB in the semaphore's waiting list.
 * signal() wakes the first waiter, or adds 1 to the value if nobody waits.
 *
 * Resources have several instances (Chapter 8). `acquire R n` blocks until n
 * instances are free, `release R n` gives them back. Each resource keeps how
 * much every process holds (allocated) and is waiting for (requested), which
 * is what a detection algorithm needs.
 *
 * Given: semaphores and resources, waiters woken in FIFO order.
 * TODO checkpoint 1: deadlock_check(), with the wait-for graph or the
 * detection algorithm for several instances.
 */
#ifndef OSSIM_SYNC_H
#define OSSIM_SYNC_H

#include "process.h"
#include "queue.h"

typedef struct ksem {
    char    name[MAX_NAME];
    int     value;
    queue_t waiters;
} ksem_t;

typedef struct resource {
    char    name[MAX_NAME];
    int     total;
    int     available;
    int     allocated[MAX_PROCS + 1];   /* indexed by pid */
    int     requested[MAX_PROCS + 1];   /* what pid is waiting for */
    queue_t waiters;
} resource_t;

extern ksem_t     sems[MAX_SEMS];
extern int        nsems;
extern resource_t resources[MAX_RES];
extern int        nres;

ksem_t     *sem_create(const char *name, int init);
ksem_t     *sem_find(const char *name);
resource_t *res_create(const char *name, int total);
resource_t *res_find(const char *name);

/* These return true if p can go on, false if p is now blocked. */
bool sync_wait(pcb_t *p, ksem_t *s);
void sync_signal(pcb_t *p, ksem_t *s);
bool res_acquire(pcb_t *p, resource_t *r, int n);
void res_release(pcb_t *p, resource_t *r, int n);

/* Called every cfg.deadlock_interval ticks. You write this one. */
void deadlock_check(void);

#endif
