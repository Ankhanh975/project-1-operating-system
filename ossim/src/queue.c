#include <limits.h>
#include "queue.h"

void q_init(queue_t *q) { q->count = 0; }
bool q_empty(const queue_t *q) { return q->count == 0; }
int  q_size(const queue_t *q) { return q->count; }

void q_push(queue_t *q, pcb_t *p)
{
    if (q->count >= MAX_PROCS) die("queue overflow");
    q->items[q->count++] = p;
}

static pcb_t *take(queue_t *q, int i)
{
    pcb_t *p = q->items[i];
    for (int j = i; j < q->count - 1; j++) q->items[j] = q->items[j + 1];
    q->count--;
    return p;
}

pcb_t *q_pop(queue_t *q)  { return q->count ? take(q, 0) : NULL; }
pcb_t *q_peek(const queue_t *q) { return q->count ? q->items[0] : NULL; }
pcb_t *q_at(const queue_t *q, int i) { return (i >= 0 && i < q->count) ? q->items[i] : NULL; }

bool q_remove(queue_t *q, pcb_t *p)
{
    for (int i = 0; i < q->count; i++)
        if (q->items[i] == p) { take(q, i); return true; }
    return false;
}

pcb_t *q_pop_min(queue_t *q, int (*key)(const pcb_t *))
{
    if (q->count == 0) return NULL;
    int best = 0;
    for (int i = 1; i < q->count; i++)
        if (key(q->items[i]) < key(q->items[best])) best = i;   /* FIFO on ties */
    return take(q, best);
}

int q_min_key(const queue_t *q, int (*key)(const pcb_t *))
{
    int best = INT_MAX;
    for (int i = 0; i < q->count; i++) {
        int k = key(q->items[i]);
        if (k < best) best = k;
    }
    return best;
}
