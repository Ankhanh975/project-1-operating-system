/*
 * disk.h: one disk with a moving head and a queue of requests.
 *
 * Requests come from page faults (DR_PAGEIN) and from `disk C` operations
 * in the workload (DR_FILEIO). The disk serves one request at a time. A
 * request takes
 *     ceil(cylinders moved * seek_cost / seek_speed) + transfer_cost
 * ticks. When it is done the kernel gets an EV_DISK_DONE event and calls
 * disk_on_complete().
 *
 * disk_submit() only adds the request to `pending`. The kernel calls
 * disk_kick() after the events of a tick and again after the tick has run,
 * so requests that arrive at the same time are all in the queue when the
 * policy chooses. The textbook examples need this.
 *
 * A disk policy is a pick() function. It gets the pending requests, the
 * head position and the direction (+1 up, -1 down, which it may change),
 * and returns the index of the request to serve next. If the head has to go
 * somewhere else first (SCAN goes to the edge of the disk before it turns
 * around, C-SCAN goes to one edge and then to the other), pick() writes
 * those cylinders in via[0] and then via[1]. Both are -1 when pick() is
 * called, which means "go straight to the request". The head movement is
 * head -> via[0] -> via[1] -> cylinder.
 *
 * Given: FCFS. TODO checkpoint 2: SSTF, SCAN, C-LOOK.
 */
#ifndef OSSIM_DISK_H
#define OSSIM_DISK_H

#include "process.h"

typedef enum { DR_PAGEIN, DR_FILEIO } dreq_type_t;

typedef struct dreq {
    dreq_type_t type;
    int    pid;
    int    page;        /* DR_PAGEIN only */
    int    frame;       /* DR_PAGEIN only: the frame mem_access() kept for it */
    int    cylinder;
    tick_t submitted;
} dreq_t;

typedef struct disk_sched {
    const char *name;
    void (*init)(void);
    int  (*pick)(const dreq_t *pending, int n, int head, int *direction, int via[2]);
} disk_sched_t;

extern disk_sched_t *disk_sched;

void disk_init(void);
void disk_select_sched(const char *name);
void disk_submit(dreq_t r);      /* adds to the queue, does not start the disk */
void disk_kick(void);            /* starts the next request if the disk is free */
void disk_on_complete(void);     /* called on EV_DISK_DONE */
int  disk_head(void);
int  disk_pending(void);
bool disk_busy(void);

/* Statistics */
extern long disk_total_movement;
extern long disk_served;
extern long disk_wait_total;     /* sum of (start - submitted) */

#endif
