/*
 * kernel.c: process lifecycle and the tick loop. Read this file first.
 *
 * What happens in one tick:
 *   1. Deliver the events due at `now` (arrivals, end of io, end of a disk
 *      request). Then disk_kick(), so the disk policy sees all requests
 *      that are waiting.
 *   2. Ask the scheduler if the running process should be preempted. If
 *      the CPU is free, dispatch the process that pick() returns.
 *   3. Count waiting and blocked time: a process that is READY now waits
 *      this tick, one that is WAITING is blocked this tick.
 *   4. Run one tick of the running process. Its current op is always cpu.
 *      It references one page. On a page fault the tick goes to the kernel
 *      and the process blocks. On a hit one tick of the burst is done, and
 *      when the burst ends kernel_advance() moves to the next op. Then
 *      disk_kick() again for requests made during the tick.
 *   5. deadlock_check() if it is time, then now++.
 *
 * Operations that take no time (signal, release, and a wait or acquire that
 * does not block) are done inside kernel_advance(), so a process is only in
 * the ready queue when it has cpu work. If the running process finishes a
 * burst and its next op is cpu (maybe after some of those zero-time ops),
 * it keeps the CPU. The scheduler can still preempt it in the next tick.
 *
 * If a page fault finds every frame still loading a page, nothing can be
 * evicted. The process then waits in frame_waiters (state WAITING, reason
 * W_PAGE) until a page-in is done or a process exits, and then tries the
 * same reference again.
 */
#include <string.h>
#include "kernel.h"
#include "event.h"
#include "sched.h"
#include "mem.h"
#include "disk.h"
#include "sync.h"

pcb_t *running = NULL;
long   ctx_switches = 0;
tick_t cpu_busy_ticks = 0, cpu_idle_ticks = 0, cpu_kernel_ticks = 0;
long   frame_waits = 0;
long   fault_service_total = 0, faults_served = 0;
int    aborted = 0;

static int   *gantt = NULL;      /* pid for each tick, 0 idle, -1 kernel */
static tick_t gantt_cap = 0;
static int    finished = 0;
static int    last_pid_on_cpu = 0;
static bool   executing = false;  /* true while we are in execute_tick() */
static pcb_t *continuing = NULL;  /* the process whose burst just ended on the CPU */
static queue_t frame_waiters;     /* page faults that found no frame to evict */
static tick_t fault_start[MAX_PROCS + 1];   /* tick of the last page fault, by pid */

/* When an op that just ended counts as finished. If the CPU finished it,
 * that is the end of this tick (now + 1). If an event finished it, the
 * event came at the start of the tick, so it is `now`. */
static tick_t completion_time(void) { return executing ? now + 1 : now; }

int gantt_pid_at(tick_t t) { return (t >= 0 && t < gantt_cap) ? gantt[t] : 0; }

static void gantt_record(int pid)
{
    if (now >= gantt_cap) return;
    gantt[now] = pid;
}

void kernel_init(void)
{
    gantt_cap = cfg.max_ticks;
    gantt = calloc((size_t)gantt_cap, sizeof(int));
    if (!gantt) die("out of memory");
    q_init(&frame_waiters);
    for (int i = 0; i < nprocs; i++) {
        pcb_t *p = &procs[i];
        p->state = P_NEW; p->cur = 0;
        p->first_run = -1; p->finish = -1;
        for (int k = 0; k < p->nops; k++) {
            p->ops[k].remaining = p->ops[k].type == OP_CPU ? p->ops[k].length : 0;
            p->ops[k].ref_pos = 0;
        }
        event_push(p->arrival, EV_ARRIVAL, p->pid);
    }
}

void kernel_make_ready(pcb_t *p)
{
    proc_set_state(p, P_READY, W_NONE);
    sched->enqueue(p);
}

static void kernel_terminate(pcb_t *p)
{
    proc_set_state(p, P_TERMINATED, W_NONE);
    p->finish = completion_time();
    mem_release(p);
    finished++;
    trace("P%d (%s) terminated", p->pid, p->name);
}

/* Kill p to break a deadlock. p must be blocked on a semaphore or a
 * resource. It leaves every waiting list, gives back what it holds (which
 * can wake other processes) and frees its frames. It does not count as
 * finished in the report. */
void kernel_abort(pcb_t *p)
{
    if (p->state != P_WAITING || (p->wait_reason != W_SEM && p->wait_reason != W_RES))
        die("kernel_abort: P%d is not blocked on a semaphore or resource", p->pid);
    trace("P%d (%s) aborted", p->pid, p->name);
    for (int i = 0; i < nsems; i++) q_remove(&sems[i].waiters, p);
    for (int i = 0; i < nres; i++) {
        q_remove(&resources[i].waiters, p);
        resources[i].requested[p->pid] = 0;
    }
    proc_set_state(p, P_TERMINATED, W_NONE);
    p->finish = -1;
    mem_release(p);
    finished++;
    aborted++;
    for (int i = 0; i < nres; i++)
        if (resources[i].allocated[p->pid] > 0)
            res_release(p, &resources[i], resources[i].allocated[p->pid]);
}

/* p has finished its current op. Move on until p has cpu work, blocks or
 * terminates. io starts at completion_time(). */
void kernel_advance(pcb_t *p)
{
    for (;;) {
        p->cur++;
        if (p->cur >= p->nops) { kernel_terminate(p); return; }
        op_t *op = &p->ops[p->cur];
        switch (op->type) {
        case OP_CPU:
            if (p == continuing) {           /* stays on the CPU */
                proc_set_state(p, P_RUNNING, W_NONE);
                running = p;
                trace("P%d continues with its next cpu burst", p->pid);
            } else {
                kernel_make_ready(p);
            }
            return;
        case OP_IO:
            proc_set_state(p, P_WAITING, W_IO);
            event_push(completion_time() + op->length, EV_IO_DONE, p->pid);
            trace("P%d starts io %d (done at t=%ld)", p->pid, op->length,
                  completion_time() + op->length);
            return;
        case OP_DISK:
            proc_set_state(p, P_WAITING, W_DISK);
            disk_submit((dreq_t){ .type = DR_FILEIO, .pid = p->pid, .cylinder = op->cylinder });
            return;
        case OP_WAIT: {
            ksem_t *s = sem_find(op->name);
            if (!s) die("P%d waits on undefined semaphore '%s'", p->pid, op->name);
            if (!sync_wait(p, s)) return;        /* blocked, signal() will advance it */
            break;                                /* go on to the next op */
        }
        case OP_SIGNAL: {
            ksem_t *s = sem_find(op->name);
            if (!s) die("P%d signals undefined semaphore '%s'", p->pid, op->name);
            sync_signal(p, s);
            break;
        }
        case OP_ACQUIRE: {
            resource_t *r = res_find(op->name);
            if (!r) die("P%d acquires undefined resource '%s'", p->pid, op->name);
            if (!res_acquire(p, r, op->length)) return;
            break;
        }
        case OP_RELEASE: {
            resource_t *r = res_find(op->name);
            if (!r) die("P%d releases undefined resource '%s'", p->pid, op->name);
            res_release(p, r, op->length);
            break;
        }
        }
    }
}

/* A process has just arrived. Its first op does not have to be cpu. */
static void kernel_admit(pcb_t *p)
{
    trace("P%d (%s) arrives", p->pid, p->name);
    p->cur = -1;                 /* so that kernel_advance() goes to op 0 */
    kernel_advance(p);
}

static void deliver_events(void)
{
    event_t ev;
    while (event_peek(&ev) && ev.time == now) {
        event_pop(&ev);
        pcb_t *p = ev.pid > 0 ? proc_by_pid(ev.pid) : NULL;
        switch (ev.type) {
        case EV_ARRIVAL:   kernel_admit(p);            break;
        case EV_IO_DONE:   trace("P%d io done", p->pid); kernel_advance(p); break;
        case EV_DISK_DONE: {
            /* If this was a page-in, measure the fault from start to end. */
            bool pagein = p && p->state == P_WAITING && p->wait_reason == W_PAGE;
            disk_on_complete();
            if (pagein) { fault_service_total += now - fault_start[p->pid]; faults_served++; }
            break;
        }
        }
    }
}

/* Is there a frame a page fault can use: a free one, or one to evict? */
static int usable_frames(void)
{
    int c = 0;
    for (int i = 0; i < nframes; i++)
        if (!frames[i].used || !frames[i].loading) c++;
    return c;
}

/* Let processes waiting for a frame try again, one for each usable frame. */
static void wake_frame_waiters(void)
{
    for (int n = usable_frames(); n > 0 && !q_empty(&frame_waiters); n--) {
        pcb_t *p = q_pop(&frame_waiters);
        trace("P%d can try its page fault again", p->pid);
        kernel_make_ready(p);
    }
}

static void dispatch(pcb_t *p)
{
    running = p;
    proc_set_state(p, P_RUNNING, W_NONE);
    p->dispatches++;
    if (p->first_run < 0) p->first_run = now;
    if (p->pid != last_pid_on_cpu) { ctx_switches++; last_pid_on_cpu = p->pid; }
    sched->on_dispatch(p);
    trace("dispatch P%d (%s), burst remaining %d", p->pid, p->name, proc_burst_remaining(p));
}

static void execute_tick(void)
{
    if (!running) { cpu_idle_ticks++; gantt_record(0); return; }

    pcb_t *p = running;
    op_t  *op = proc_cur_op(p);
    if (!op || op->type != OP_CPU) die("P%d is RUNNING without a CPU op", p->pid);

    int page = proc_peek_ref(p), frame;
    if (cfg.paging && !p->page_table[page].valid && usable_frames() == 0) {
        /* Every frame is loading a page, so nothing can be evicted. The
         * kernel spends the tick on the fault and the process waits. */
        cpu_kernel_ticks++;
        gantt_record(-1);
        frame_waits++;
        trace("P%d page %d FAULT: every frame is loading, P%d waits", p->pid, page, p->pid);
        proc_set_state(p, P_WAITING, W_PAGE);
        q_push(&frame_waiters, p);
        running = NULL;
        return;
    }
    if (cfg.paging && mem_access(p, page, &frame) == MEM_FAULT) {
        /* The kernel spends this tick on the fault. */
        fault_start[p->pid] = now;
        cpu_kernel_ticks++;
        gantt_record(-1);
        proc_set_state(p, P_WAITING, W_PAGE);
        disk_submit((dreq_t){ .type = DR_PAGEIN, .pid = p->pid, .page = page, .frame = frame,
                              .cylinder = (p->pid * 31 + page * 17) % cfg.cylinders });
        running = NULL;
        return;
    }

    proc_consume_ref(p);
    op->remaining--;
    p->cpu_time++;
    cpu_busy_ticks++;
    gantt_record(p->pid);
    sched->on_tick(p);

    if (op->remaining == 0) {
        trace("P%d finished cpu burst", p->pid);
        running = NULL;
        executing = true;
        continuing = p;
        kernel_advance(p);
        continuing = NULL;
        executing = false;
    }
}

static void account(void)
{
    for (int i = 0; i < nprocs; i++) {
        if (procs[i].state == P_READY)   procs[i].wait_time++;
        if (procs[i].state == P_WAITING) procs[i].block_time++;
    }
}

void kernel_run(void)
{
    while (finished < nprocs && now < cfg.max_ticks) {
        deliver_events();
        wake_frame_waiters();
        disk_kick();          /* after all the events, so they all compete */

        if (running && sched->should_preempt(running)) {
            trace("preempt P%d", running->pid);
            kernel_make_ready(running);
            running = NULL;
        }
        if (!running) {
            pcb_t *next = sched->pick();
            if (next) dispatch(next);
        }

        account();
        execute_tick();
        disk_kick();          /* requests made during this tick */

        if (cfg.deadlock_interval > 0 && now > 0 && now % cfg.deadlock_interval == 0)
            deadlock_check();
        now++;
    }
    if (finished < nprocs)
        printf("WARNING: %d of %d processes did not finish within %ld ticks "
               "(deadlock? starvation? --max-ticks too small?)\n",
               nprocs - finished, nprocs, cfg.max_ticks);
}
