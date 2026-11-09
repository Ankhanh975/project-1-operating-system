/*
 * sim.h: settings from the command line, the clock, and limits.
 *
 * COMP2040 Operating Systems, group project (ossim).
 * The simulation moves one tick at a time. `now` is the current tick.
 * Things that will happen later (arrivals, end of io, end of a disk
 * request) wait in the event queue, see event.h.
 */
#ifndef OSSIM_SIM_H
#define OSSIM_SIM_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_PROCS      64      /* processes in a workload */
#define MAX_OPS        128     /* operations per process */
#define MAX_REFS       512     /* page references in one cpu op */
#define MAX_NAME       32      /* length of process, semaphore and resource names */
#define MAX_SEMS       32
#define MAX_RES        32
#define MAX_FRAMES     4096
#define MAX_PAGES      1024    /* virtual pages per process */
#define MAX_DISK_REQ   1024    /* requests waiting for the disk */
#define MAX_EVENTS     8192
#define MAX_TICKS_DEF  100000  /* default --max-ticks, stops runs that never end */

typedef long tick_t;

typedef struct config {
    const char *workload_path;
    const char *sched_name;      /* fcfs, rr, sjf, srtf, prio, mlfq */
    int         quantum;         /* RR quantum, and the base quantum of MLFQ */
    int         frames;
    const char *replace_name;    /* fifo, lru, clock */
    const char *disk_name;       /* fcfs, sstf, scan, clook */
    int         cylinders;       /* cylinders are 0 .. cylinders-1 */
    int         head;            /* where the head starts */
    int         direction;       /* where it is moving at the start: +1 up, -1 down */
    int         seek_cost;       /* seek ticks = ceil(cylinders moved * seek_cost / seek_speed) */
    int         seek_speed;
    int         transfer_cost;   /* ticks to transfer once the head is there */
    tick_t      max_ticks;       /* stop after this many ticks */
    int         deadlock_interval; /* call deadlock_check() every N ticks, 0 = never */
    bool        paging;          /* false: every reference is a hit */
    bool        trace;           /* --trace */
    bool        gantt;           /* --gantt */
    const char *csv_path;        /* --csv */
} config_t;

extern config_t cfg;   /* filled in by main() */
extern tick_t   now;   /* the current tick */

/* Print the message to stderr and exit(1). */
void die(const char *fmt, ...);

/* Print a trace line, only with --trace. */
void trace(const char *fmt, ...);

#endif
