/*
 * sched.h: the CPU scheduler interface.
 *
 * The kernel calls these at fixed points of every tick:
 *   enqueue(p)          p has become READY (arrival, end of io, preempted,
 *                       woken up)
 *   should_preempt(r)   before r runs this tick. Return true to put r back
 *                       in the ready queue.
 *   pick()              the CPU is free. Remove and return the next process.
 *   on_dispatch(p)      p has just got the CPU (reset its quantum, etc.)
 *   on_tick(r)          r has just run one tick
 *
 * Given: FCFS and RR. TODO checkpoint 1: SJF, SRTF, PRIO, MLFQ.
 * The project handout shows how to add one.
 */
#ifndef OSSIM_SCHED_H
#define OSSIM_SCHED_H

#include "process.h"

typedef struct scheduler {
    const char *name;
    void   (*init)(void);
    void   (*enqueue)(pcb_t *p);
    bool   (*should_preempt)(pcb_t *running);
    pcb_t *(*pick)(void);
    void   (*on_dispatch)(pcb_t *p);
    void   (*on_tick)(pcb_t *running);
    int    (*ready_count)(void);
} scheduler_t;

extern scheduler_t *sched;   /* the scheduler chosen with --sched */

/* Choose the scheduler by name. Stops with an error if the name is unknown. */
void sched_select(const char *name);

#endif
