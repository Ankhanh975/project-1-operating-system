/*
 * sync.c: semaphores, resources and multi-instance deadlock detection.
 */
#include <string.h>
#include "sync.h"
#include "kernel.h"

ksem_t     sems[MAX_SEMS];
int        nsems = 0;
resource_t resources[MAX_RES];
int        nres = 0;
static bool last_deadlocked[MAX_PROCS + 1];

ksem_t *sem_find(const char *name)
{
    for (int i = 0; i < nsems; i++) if (strcmp(sems[i].name, name) == 0) return &sems[i];
    return NULL;
}

ksem_t *sem_create(const char *name, int init)
{
    if (sem_find(name)) die("semaphore '%s' defined twice", name);
    if (nsems >= MAX_SEMS) die("too many semaphores");
    ksem_t *s = &sems[nsems++];
    strncpy(s->name, name, MAX_NAME - 1);
    s->value = init;
    q_init(&s->waiters);
    return s;
}

resource_t *res_find(const char *name)
{
    for (int i = 0; i < nres; i++) if (strcmp(resources[i].name, name) == 0) return &resources[i];
    return NULL;
}

resource_t *res_create(const char *name, int total)
{
    if (res_find(name)) die("resource '%s' defined twice", name);
    if (nres >= MAX_RES) die("too many resources");
    resource_t *r = &resources[nres++];
    memset(r, 0, sizeof(*r));
    strncpy(r->name, name, MAX_NAME - 1);
    r->total = r->available = total;
    q_init(&r->waiters);
    return r;
}

/* wait(S): if S > 0, take one and go on. Otherwise block in S's list. */
bool sync_wait(pcb_t *p, ksem_t *s)
{
    if (s->value > 0) {
        s->value--;
        trace("P%d wait(%s) -> %d", p->pid, s->name, s->value);
        return true;
    }
    trace("P%d wait(%s) blocks", p->pid, s->name);
    q_push(&s->waiters, p);
    proc_set_state(p, P_WAITING, W_SEM);
    return false;
}

/* signal(S): wake the first waiter, or S++ if nobody waits. */
void sync_signal(pcb_t *p, ksem_t *s)
{
    pcb_t *w = q_pop(&s->waiters);
    if (w) {
        trace("P%d signal(%s) wakes P%d", p->pid, s->name, w->pid);
        kernel_advance(w);                /* w goes past its `wait` op */
    } else {
        s->value++;
        trace("P%d signal(%s) -> %d", p->pid, s->name, s->value);
    }
}

bool res_acquire(pcb_t *p, resource_t *r, int n)
{
    if (n > r->total) die("P%d requests %d of '%s' but only %d exist", p->pid, n, r->name, r->total);
    if (r->available >= n && q_empty(&r->waiters)) {   /* no overtaking the waiters */
        r->available -= n;
        r->allocated[p->pid] += n;
        trace("P%d acquire(%s,%d) -> available %d", p->pid, r->name, n, r->available);
        return true;
    }
    r->requested[p->pid] = n;
    q_push(&r->waiters, p);
    proc_set_state(p, P_WAITING, W_RES);
    trace("P%d acquire(%s,%d) blocks (available %d)", p->pid, r->name, n, r->available);
    return false;
}

void res_release(pcb_t *p, resource_t *r, int n)
{
    if (r->allocated[p->pid] < n)
        die("P%d releases %d of '%s' but holds %d", p->pid, n, r->name, r->allocated[p->pid]);
    r->allocated[p->pid] -= n;
    r->available += n;
    trace("P%d release(%s,%d) -> available %d", p->pid, r->name, n, r->available);

    /* Give the instances to the waiters in FIFO order while they fit. */
    while (!q_empty(&r->waiters)) {
        pcb_t *w = q_peek(&r->waiters);
        int need = r->requested[w->pid];
        if (need > r->available) break;
        q_pop(&r->waiters);
        r->available -= need;
        r->allocated[w->pid] += need;
        r->requested[w->pid] = 0;
        trace("P%d granted %d of '%s'", w->pid, need, r->name);
        kernel_advance(w);
    }
}

/*
 * TODO checkpoint 1: deadlock detection.
 * The kernel calls this every cfg.deadlock_interval ticks (never if it is 0).
 * Two ways to do it, choose one:
 *   - Wait-for graph (one instance per resource): build it from
 *     resources[].waiters, requested[] and allocated[]. A cycle is a deadlock.
 *   - Detection algorithm of Sec. 8.6.2 (several instances), with
 *     Available = r->available, Allocation = r->allocated and
 *     Request = r->requested.
 * Print the deadlocked processes in exactly this format, pids from small to
 * large, because tests/run.sh looks for it:
 *     DEADLOCK t=<now> pids: 1 2
 * After that you can kill a victim with kernel_abort(p) (see kernel.h) or
 * only report it and let --max-ticks end the run. If you only report, print
 * each deadlock once. workloads/deadlock.txt must be detected.
 */
void deadlock_check(void)
{
    int work[MAX_RES];
    bool finish[MAX_PROCS + 1];
    bool deadlocked[MAX_PROCS + 1];

    for (int r = 0; r < nres; r++) work[r] = resources[r].available;
    for (int pid = 1; pid <= nprocs; pid++) {
        finish[pid] = false;
        deadlocked[pid] = false;
    }

    bool changed;
    do {
        changed = false;
        for (int pid = 1; pid <= nprocs; pid++) {
            if (finish[pid]) continue;

            bool can_finish = true;
            for (int r = 0; r < nres; r++) {
                if (resources[r].requested[pid] > work[r]) {
                    can_finish = false;
                    break;
                }
            }
            if (!can_finish) continue;

            finish[pid] = true;
            changed = true;
            for (int r = 0; r < nres; r++)
                work[r] += resources[r].allocated[pid];
        }
    } while (changed);

    bool any = false;
    for (int pid = 1; pid <= nprocs; pid++) {
        pcb_t *p = proc_by_pid(pid);
        if (!finish[pid] && p->state == P_WAITING) {
            deadlocked[pid] = true;
            any = true;
        }
    }
    if (!any) {
        memset(last_deadlocked, 0, sizeof(last_deadlocked));
        return;
    }

    bool same_as_previous = true;
    for (int pid = 1; pid <= nprocs; pid++) {
        if (deadlocked[pid] != last_deadlocked[pid]) {
            same_as_previous = false;
            break;
        }
    }
    if (same_as_previous) return;

    printf("DEADLOCK t=%ld pids:", now);
    for (int pid = 1; pid <= nprocs; pid++)
        if (deadlocked[pid]) printf(" %d", pid);
    putchar('\n');

    for (int pid = 1; pid <= nprocs; pid++)
        last_deadlocked[pid] = deadlocked[pid];
}
