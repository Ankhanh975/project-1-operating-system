/*
 * process.h: operations, processes and the PCB.
 *
 * A process is a list of operations, done in order:
 *   cpu N [refs=...]                run N ticks, one page reference per tick
 *   io N                            wait N ticks for an I/O device
 *   disk C                          wait for a request on cylinder C
 *   wait S / signal S               semaphore operations
 *   acquire R [n] / release R [n]   take or give back n instances of R
 *
 * The PCB has the same parts as in Chapter 3: state, scheduling
 * information, memory information (the page table) and accounting.
 */
#ifndef OSSIM_PROCESS_H
#define OSSIM_PROCESS_H

#include "sim.h"

typedef enum {
    OP_CPU, OP_IO, OP_DISK, OP_WAIT, OP_SIGNAL, OP_ACQUIRE, OP_RELEASE
} op_type_t;

typedef struct op {
    op_type_t type;
    int       length;         /* ticks for cpu/io, number of instances for acquire/release */
    int       remaining;      /* cpu ticks left */
    int       cylinder;       /* for disk */
    char      name[MAX_NAME]; /* semaphore or resource name */
    int       refs[MAX_REFS]; /* page references of a cpu op */
    int       nrefs;          /* 0 means pages 0, 1, ..., pages-1 */
    int       ref_pos;        /* next reference */
} op_t;

typedef enum { P_NEW, P_READY, P_RUNNING, P_WAITING, P_TERMINATED } pstate_t;

typedef enum {
    W_NONE, W_IO, W_PAGE, W_DISK, W_SEM, W_RES
} wait_reason_t;

typedef struct pte {          /* page-table entry */
    int  frame;
    bool valid;               /* true if the page is in `frame` */
} pte_t;

typedef struct pcb {
    int      pid;
    char     name[MAX_NAME];
    tick_t   arrival;
    int      priority;        /* smaller number = higher priority */
    int      pages;           /* number of virtual pages */

    op_t     ops[MAX_OPS];
    int      nops;
    int      cur;             /* index of the current operation */

    pstate_t      state;
    wait_reason_t wait_reason;

    /* Scheduling. Your schedulers can use these as they like. */
    int      quantum_used;    /* ticks run since the last dispatch (RR) */
    int      level;           /* MLFQ level */
    tick_t   ready_since;     /* when it last became READY */

    /* Memory */
    pte_t    page_table[MAX_PAGES];

    /* Accounting */
    tick_t   first_run;       /* -1 until the first dispatch */
    tick_t   finish;          /* -1 until it terminates */
    tick_t   cpu_time;        /* ticks run on the CPU */
    tick_t   wait_time;       /* ticks spent READY */
    tick_t   block_time;      /* ticks spent WAITING */
    int      total_cpu;       /* sum of all cpu lengths */
    int      page_faults;
    int      disk_requests;
    int      dispatches;
} pcb_t;

extern pcb_t procs[MAX_PROCS];
extern int   nprocs;

pcb_t *proc_by_pid(int pid);
void   proc_set_state(pcb_t *p, pstate_t s, wait_reason_t why);
op_t  *proc_cur_op(pcb_t *p);

/* Ticks left in the current cpu burst (0 if the current op is not cpu). */
int    proc_burst_remaining(const pcb_t *p);
/* Ticks left in all the cpu ops of the process. */
int    proc_total_remaining(const pcb_t *p);

/* The page the current cpu op references this tick. Does not move on. */
int    proc_peek_ref(pcb_t *p);
/* Move on to the next reference, after the page was found in memory. */
void   proc_consume_ref(pcb_t *p);

const char *pstate_name(pstate_t s);
const char *op_name(op_type_t t);

#endif
