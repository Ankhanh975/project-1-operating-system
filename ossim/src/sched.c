/*
 * sched.c: CPU schedulers.
 *
 * FCFS and RR are given. SJF and SRTF remain checkpoint work; priority and
 * MLFQ are implemented below.
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
 * Priority with preemption (smaller number = higher priority), and RR
 * between processes of the same priority. Low priorities can starve; aging
 * is a stretch goal. */
static queue_t prio_q;
static int prio_key(const pcb_t *p) { return p->priority; }

static void prio_init(void)            { q_init(&prio_q); }
static void prio_enqueue(pcb_t *p)     { q_push(&prio_q, p); }
static pcb_t *prio_pick(void)          { return q_pop_min(&prio_q, prio_key); }
static void prio_on_dispatch(pcb_t *p) { p->quantum_used = 0; }
static void prio_on_tick(pcb_t *p)     { p->quantum_used++; }
static int prio_ready_count(void)      { return q_size(&prio_q); }

static bool prio_should_preempt(pcb_t *r)
{
    if (q_empty(&prio_q)) return false;
    int best = q_min_key(&prio_q, prio_key);
    if (best < r->priority) return true;
    if (best == r->priority && r->quantum_used >= cfg.quantum) {
        r->quantum_used = 0;
        return true;
    }
    return false;
}

static scheduler_t sched_prio = {
    "prio", prio_init, prio_enqueue, prio_should_preempt, prio_pick,
    prio_on_dispatch, prio_on_tick, prio_ready_count
};

/* MLFQ
 * Three-level feedback queue with RR quanta q, 2q and 4q. A process that
 * uses its whole quantum goes down one level. A process that blocks before
 * the quantum ends stays where it is. Every 20 ticks, everyone returns to
 * level 0 so nobody starves. */
static queue_t mlfq_q[3];

static void mlfq_init(void)
{
    for (int i = 0; i < 3; i++) q_init(&mlfq_q[i]);
}

static void mlfq_enqueue(pcb_t *p)
{
    if (p->level < 0) p->level = 0;
    if (p->level > 2) p->level = 2;
    q_push(&mlfq_q[p->level], p);
}

static pcb_t *mlfq_pick(void)
{
    for (int i = 0; i < 3; i++)
        if (!q_empty(&mlfq_q[i])) return q_pop(&mlfq_q[i]);
    return NULL;
}

static void mlfq_on_dispatch(pcb_t *p) { p->quantum_used = 0; }
static int mlfq_ready_count(void)
{
    return q_size(&mlfq_q[0]) + q_size(&mlfq_q[1]) + q_size(&mlfq_q[2]);
}

static void mlfq_boost(void)
{
    for (int level = 1; level < 3; level++) {
        while (!q_empty(&mlfq_q[level])) {
            pcb_t *p = q_pop(&mlfq_q[level]);
            p->level = 0;
            p->quantum_used = 0;
            q_push(&mlfq_q[0], p);
        }
    }
}

static void mlfq_on_tick(pcb_t *p)
{
    p->quantum_used++;
    if ((now + 1) % 20 == 0) {
        p->level = 0;
        p->quantum_used = 0;
        mlfq_boost();
    }
}

static bool mlfq_should_preempt(pcb_t *r)
{
    int highest = -1;
    for (int i = 0; i < 3; i++) {
        if (!q_empty(&mlfq_q[i])) {
            highest = i;
            break;
        }
    }
    if (highest >= 0 && highest < r->level) return true;

    int quantum = cfg.quantum * (1 << r->level);
    if (r->quantum_used < quantum) return false;
    r->quantum_used = 0;
    if (r->level < 2) r->level++;
    return highest >= 0;
}

static scheduler_t sched_mlfq = {
    "mlfq", mlfq_init, mlfq_enqueue, mlfq_should_preempt, mlfq_pick,
    mlfq_on_dispatch, mlfq_on_tick, mlfq_ready_count
};

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
