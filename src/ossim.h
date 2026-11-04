#ifndef OSSIM_H
#define OSSIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t SimTime;

typedef enum {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED_SEMAPHORE,
    PROCESS_BLOCKED_RESOURCE,
    PROCESS_TERMINATED
} ProcessState;

typedef enum {
    SCHED_FCFS,
    SCHED_RR,
    SCHED_SJF,
    SCHED_SRTF,
    SCHED_PRIORITY_RR,
    SCHED_MLFQ
} SchedulingPolicy;

typedef enum {
    DEADLOCK_WAIT_FOR_GRAPH,
    DEADLOCK_MULTI_INSTANCE
} DeadlockDetectionPolicy;

typedef enum {
    STEP_CPU_BURST,
    STEP_SEMAPHORE_WAIT,
    STEP_SEMAPHORE_SIGNAL,
    STEP_RESOURCE_REQUEST,
    STEP_RESOURCE_RELEASE
} ProcessStepKind;

typedef struct {
    ProcessStepKind kind;
    union {
        SimTime cpu_ticks;
        size_t semaphore_index;
        struct {
            size_t resource_index;
            uint32_t instances;
        } resource;
    } value;
} ProcessStep;

typedef struct Process Process;

typedef struct {
    Process *head;
    Process *tail;
    size_t length;
} ProcessQueue;

struct Process {
    uint32_t pid;
    size_t resource_row;
    SimTime arrival_time;
    int32_t priority;
    ProcessState state;

    ProcessStep *steps;
    size_t step_count;
    size_t next_step;
    SimTime cpu_remaining;

    uint8_t mlfq_level;
    SimTime quantum_remaining;
    uint64_t ready_order;
    Process *queue_next;

    SimTime first_run_time;
    SimTime completion_time;
    SimTime waiting_time;
    SimTime cpu_time;
    bool has_started;
};

typedef struct {
    SchedulingPolicy policy;
    ProcessQueue ready;
    ProcessQueue mlfq_ready[3];
    Process *running;
    SimTime round_robin_quantum;
    SimTime mlfq_quantum;
    SimTime next_mlfq_boost;
    SimTime mlfq_boost_period;
    uint64_t next_ready_order;
} Scheduler;

typedef struct {
    const char *name;
    int64_t value;
    ProcessQueue waiters;
} Semaphore;

typedef struct {
    const char *name;
    uint32_t total_instances;
} ResourceType;

typedef struct {
    size_t process_count;
    size_t type_count;
    ResourceType *types;
    uint32_t *available;
    /* Matrices are row-major: process_count rows by type_count columns. */
    uint32_t *allocation_matrix;
    uint32_t *request_matrix;
} ResourceState;

typedef struct {
    size_t *process_indices;
    size_t count;
} DeadlockReport;

typedef struct {
    SimTime now;
    Process *processes;
    size_t process_count;
    Semaphore *semaphores;
    size_t semaphore_count;
    Scheduler scheduler;
    DeadlockDetectionPolicy deadlock_policy;
    ResourceState resources;
} Simulator;

#endif
