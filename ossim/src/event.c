/* Event queue as a binary min-heap, ordered by time and then seq. */
#include "event.h"

static event_t heap[MAX_EVENTS];
static int     n = 0;
static long    seq = 0;

static bool before(const event_t *a, const event_t *b)
{
    if (a->time != b->time) return a->time < b->time;
    return a->seq < b->seq;
}

static void swap(int i, int j)
{
    event_t t = heap[i]; heap[i] = heap[j]; heap[j] = t;
}

void event_init(void) { n = 0; seq = 0; }

void event_push(tick_t time, ev_type_t type, int pid)
{
    if (n >= MAX_EVENTS) die("event queue overflow");
    if (time < now) die("event scheduled in the past (t=%ld < now=%ld)", time, now);
    heap[n] = (event_t){ .time = time, .type = type, .pid = pid, .seq = seq++ };
    int i = n++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (!before(&heap[i], &heap[parent])) break;
        swap(i, parent);
        i = parent;
    }
}

bool event_peek(event_t *out)
{
    if (n == 0) return false;
    *out = heap[0];
    return true;
}

bool event_pop(event_t *out)
{
    if (n == 0) return false;
    *out = heap[0];
    heap[0] = heap[--n];
    int i = 0;
    for (;;) {
        int l = 2 * i + 1, r = l + 1, m = i;
        if (l < n && before(&heap[l], &heap[m])) m = l;
        if (r < n && before(&heap[r], &heap[m])) m = r;
        if (m == i) break;
        swap(i, m);
        i = m;
    }
    return true;
}

int event_count(void) { return n; }

const char *ev_name(ev_type_t t)
{
    switch (t) {
    case EV_ARRIVAL:   return "ARRIVAL";
    case EV_IO_DONE:   return "IO_DONE";
    case EV_DISK_DONE: return "DISK_DONE";
    }
    return "?";
}
