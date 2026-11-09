/*
 * sched.c: CPU schedulers.
 *
 * FCFS and RR are given. TODO checkpoint 1: sjf, srtf, prio, mlfq.
 * Each one is a scheduler_t with the same hooks as FCFS and RR. There are
 * notes above each stub. Functions you will probably need:
 *   proc_burst_remaining(p)   ticks left in p's current cpu burst
 *   proc_total_remaining(p)   cpu ticks left in the whole process
 *   q_pop_min(&q, key)        remove the element with the smallest key
 *   q_min_key(&q, key)        the smallest key, without removing (for preemption)
 */
#include <string.h>
#include "sched.h"
#include "queue.h"

scheduler_t *sched = NULL;

/* FCFS */
static queue_t fcfs_q;

static void   fcfs_init(void)                { q_init(&fcfs_q); }
static void   fcfs_enqueue(pcb_t *p)         { q_push(&fcfs_q, p); }
static bool   fcfs_should_preempt(pcb_t *r)  { (void)r; return false; }
static pcb_t *fcfs_pick(void)                { return q_pop(&fcfs_q); }
static void   fcfs_on_dispatch(pcb_t *p)     { (void)p; }
static void   fcfs_on_tick(pcb_t *r)         { (void)r; }
static int    fcfs_ready_count(void)         { return q_size(&fcfs_q); }

static scheduler_t sched_fcfs = {
    "fcfs", fcfs_init, fcfs_enqueue, fcfs_should_preempt, fcfs_pick,
    fcfs_on_dispatch, fcfs_on_tick, fcfs_ready_count
};

/* RR */
static queue_t rr_q;

static void   rr_init(void)            { q_init(&rr_q); }
static void   rr_enqueue(pcb_t *p)     { q_push(&rr_q, p); }
static pcb_t *rr_pick(void)            { return q_pop(&rr_q); }
static void   rr_on_dispatch(pcb_t *p) { p->quantum_used = 0; }
static void   rr_on_tick(pcb_t *r)     { r->quantum_used++; }
static int    rr_ready_count(void)     { return q_size(&rr_q); }

/* Preempt when the quantum is used up and someone else is ready. If nobody
 * is ready, the process keeps the CPU and starts a new quantum, so a process
 * that arrives later waits until that quantum is over. */
static bool rr_should_preempt(pcb_t *r)
{
    if (r->quantum_used < cfg.quantum) return false;
    r->quantum_used = 0;
    return !q_empty(&rr_q);
}

static scheduler_t sched_rr = {
    "rr", rr_init, rr_enqueue, rr_should_preempt, rr_pick,
    rr_on_dispatch, rr_on_tick, rr_ready_count
};

/* SJF
 * TODO: shortest job first, non-preemptive.
 *   pick(): q_pop_min(&q, key) with key = proc_burst_remaining
 *   should_preempt(): always false
 * For your report: ossim knows the exact length of the next burst, a real
 * OS does not. How would you add exponential averaging (Sec. 5.3.2)? */
static void sjf_init(void) { die("scheduler 'sjf' is not implemented yet (src/sched.c)"); }
static scheduler_t sched_sjf = { "sjf", sjf_init, NULL, NULL, NULL, NULL, NULL, NULL };

/* SRTF
 * TODO: SJF with preemption. should_preempt(r) is true when a READY process
 * has less left of its burst than r, that is q_min_key(&q, key) < key(r). */
static void srtf_init(void) { die("scheduler 'srtf' is not implemented yet (src/sched.c)"); }
static scheduler_t sched_srtf = { "srtf", srtf_init, NULL, NULL, NULL, NULL, NULL, NULL };

/* PRIO
 * TODO: priority with preemption (smaller number = higher priority), and RR
 * between processes of the same priority. Low priorities can starve; aging
 * is a stretch goal. */
static void prio_init(void) { die("scheduler 'prio' is not implemented yet (src/sched.c)"); }
static scheduler_t sched_prio = { "prio", prio_init, NULL, NULL, NULL, NULL, NULL, NULL };

/* MLFQ
 * TODO: multilevel feedback queue with 3 levels, RR with quantum q, 2q and
 * 4q. A process that uses its whole quantum goes down one level. A process
 * that blocks before the quantum ends stays where it is. Every 20 ticks,
 * move everyone back to level 0 so nobody starves. Use p->level. */
static void mlfq_init(void) { die("scheduler 'mlfq' is not implemented yet (src/sched.c)"); }
static scheduler_t sched_mlfq = { "mlfq", mlfq_init, NULL, NULL, NULL, NULL, NULL, NULL };

static scheduler_t *all[] = {
    &sched_fcfs, &sched_rr, &sched_sjf, &sched_srtf, &sched_prio, &sched_mlfq
};

void sched_select(const char *name)
{
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (strcmp(all[i]->name, name) == 0) {
            sched = all[i];
            sched->init();
            return;
        }
    }
    die("unknown scheduler '%s' (fcfs, rr, sjf, srtf, prio, mlfq)", name);
}
