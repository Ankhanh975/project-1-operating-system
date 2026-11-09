/*
 * kernel.h: process lifecycle and the main loop.
 *
 * kernel_make_ready(p)   put p in the ready queue (state READY).
 * kernel_advance(p)      p has finished its current operation. Go to the
 *                        next one: do the ones that take no time (signal,
 *                        wait or acquire that does not block), start io or
 *                        disk, or terminate p. Afterwards p is READY,
 *                        RUNNING, WAITING or TERMINATED.
 * kernel_run()           the tick loop. Returns when every process has
 *                        finished or after cfg.max_ticks ticks.
 * kernel_abort(p)        kill p to break a deadlock (p must be blocked on a
 *                        semaphore or resource). Use it in deadlock_check()
 *                        if you recover by killing a victim.
 */
#ifndef OSSIM_KERNEL_H
#define OSSIM_KERNEL_H

#include "process.h"

void kernel_init(void);
void kernel_make_ready(pcb_t *p);
void kernel_advance(pcb_t *p);
void kernel_run(void);
void kernel_abort(pcb_t *p);

/* For metrics.c and the trace. Do not change these from outside kernel.c. */
extern pcb_t *running;
extern long   ctx_switches;
extern tick_t cpu_busy_ticks, cpu_idle_ticks, cpu_kernel_ticks;
extern int    gantt_pid_at(tick_t t);   /* pid, 0 = idle, -1 = page fault (kernel) */
extern long   frame_waits;              /* faults that had to wait for a frame */
extern long   fault_service_total;      /* sum of (page-in done - fault) */
extern long   faults_served;
extern int    aborted;                  /* processes killed by kernel_abort() */

#endif
