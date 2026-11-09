/*
 * event.h: queue of future events, ordered by time.
 *
 * An event with time t is delivered at the start of tick t. Events with the
 * same time come out in the order they were added. The kernel takes out
 * everything due at `now` before it makes the scheduling decision.
 */
#ifndef OSSIM_EVENT_H
#define OSSIM_EVENT_H

#include "sim.h"

typedef enum {
    EV_ARRIVAL,    /* process `pid` enters the system */
    EV_IO_DONE,    /* the io burst of `pid` is over */
    EV_DISK_DONE,  /* the disk finished the request it was serving */
} ev_type_t;

typedef struct event {
    tick_t    time;
    ev_type_t type;
    int       pid;    /* -1 if the event is not about one process */
    long      seq;    /* order of insertion, used when times are equal */
} event_t;

void event_init(void);
void event_push(tick_t time, ev_type_t type, int pid);
bool event_peek(event_t *out);   /* look at the earliest event without removing it */
bool event_pop(event_t *out);
int  event_count(void);
const char *ev_name(ev_type_t t);

#endif
