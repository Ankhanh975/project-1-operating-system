#include <string.h>
#include "process.h"

pcb_t procs[MAX_PROCS];
int   nprocs = 0;

pcb_t *proc_by_pid(int pid)
{
    for (int i = 0; i < nprocs; i++)
        if (procs[i].pid == pid) return &procs[i];
    return NULL;
}

void proc_set_state(pcb_t *p, pstate_t s, wait_reason_t why)
{
    if (p->state != s)
        trace("P%d (%s) %s -> %s", p->pid, p->name,
              pstate_name(p->state), pstate_name(s));
    p->state = s;
    p->wait_reason = (s == P_WAITING) ? why : W_NONE;
    if (s == P_READY) p->ready_since = now;
}

op_t *proc_cur_op(pcb_t *p)
{
    return (p->cur < p->nops) ? &p->ops[p->cur] : NULL;
}

int proc_burst_remaining(const pcb_t *p)
{
    if (p->cur >= p->nops) return 0;
    const op_t *op = &p->ops[p->cur];
    return op->type == OP_CPU ? op->remaining : 0;
}

int proc_total_remaining(const pcb_t *p)
{
    int sum = 0;
    for (int i = p->cur; i < p->nops; i++)
        if (p->ops[i].type == OP_CPU) sum += p->ops[i].remaining;
    return sum;
}

int proc_peek_ref(pcb_t *p)
{
    op_t *op = proc_cur_op(p);
    if (!op || op->type != OP_CPU) return 0;
    if (op->nrefs == 0)                       /* no refs=: pages 0..pages-1 in a loop */
        return p->pages > 0 ? op->ref_pos % p->pages : 0;
    return op->refs[op->ref_pos % op->nrefs];
}

void proc_consume_ref(pcb_t *p)
{
    op_t *op = proc_cur_op(p);
    if (op && op->type == OP_CPU) op->ref_pos++;
}

const char *pstate_name(pstate_t s)
{
    switch (s) {
    case P_NEW:        return "NEW";
    case P_READY:      return "READY";
    case P_RUNNING:    return "RUNNING";
    case P_WAITING:    return "WAITING";
    case P_TERMINATED: return "TERMINATED";
    }
    return "?";
}

const char *op_name(op_type_t t)
{
    switch (t) {
    case OP_CPU:     return "cpu";
    case OP_IO:      return "io";
    case OP_DISK:    return "disk";
    case OP_WAIT:    return "wait";
    case OP_SIGNAL:  return "signal";
    case OP_ACQUIRE: return "acquire";
    case OP_RELEASE: return "release";
    }
    return "?";
}
