# Checkpoint 1 plan and design note

## Goal

Complete the process, scheduling, and synchronization portion of `ossim`
without changing the public simulator interface. The simulator remains
tick-driven: the kernel owns process state transitions and calls a selected
policy through `scheduler_t`.

This preliminary version defines the structures, invariants, and validation
plan only. It does not implement the four missing schedulers or deadlock
detection.

## Scope

### Included

1. SJF (non-preemptive)
2. SRTF
3. Preemptive priority scheduling, with round-robin ordering among equal
   priorities
4. Three-level MLFQ with base quantum `q`, then `2q`, then `4q`
5. Periodic MLFQ boost
6. Multi-instance deadlock detection
7. Hand-computed tests and a short comparison using `workloads/arrivals.txt`

### Deferred

Paging replacement, disk scheduling, workload generation, and the optional
project theme belong to later checkpoints.

## Existing structures

### Process control block

`pcb_t` in `include/process.h` already stores the state needed by Checkpoint 1:

- `state` and `wait_reason` identify whether a process is ready, running, or
  blocked.
- `priority` is ordered with smaller values representing higher priority.
- `quantum_used` tracks time used by the current dispatch.
- `level` stores the MLFQ queue level.
- `ready_since` supports waiting-time accounting and future aging experiments.

The kernel remains responsible for changing process states. Scheduler code
should only maintain ready-queue membership and policy metadata.

### Ready queues

`queue_t` stores PCB pointers and provides FIFO operations plus
`q_pop_min()`/`q_min_key()`. The planned queue layout is:

- SJF: one queue, selected by `proc_burst_remaining()`.
- SRTF: one queue, selected by remaining current-burst time.
- PRIO: one queue, selected by priority; equal priorities retain FIFO order.
- MLFQ: three FIFO queues indexed by `pcb_t.level`.

The queue helpers preserve insertion order when keys tie. This gives
deterministic results for hand-computed tests.

### Synchronization and deadlock inputs

`resource_t` already contains the matrices required by the
multi-instance detection algorithm:

- `available`: currently free instances
- `allocated[pid]`: instances held by each process
- `requested[pid]`: outstanding request for each blocked process

Resource waiters remain FIFO. Detection will inspect resource state without
changing ownership or waking processes.

## Policy rules and invariants

### SJF

- Select the ready process with the shortest remaining current CPU burst.
- Never preempt the running process.
- Resolve equal burst lengths by ready-queue order.

### SRTF

- Select the shortest remaining current CPU burst.
- Preempt only when a ready process has a strictly shorter burst than the
  running process.
- Equal remaining times do not cause a context switch.

### PRIO

- Smaller `priority` values run first.
- Preempt when a ready process has a strictly higher priority.
- Equal priorities use round-robin behavior with `cfg.quantum`.

### MLFQ

- Level 0 uses `q`, level 1 uses `2q`, and level 2 uses `4q`.
- Always select the highest non-empty level.
- A process using its full quantum moves down one level, capped at level 2.
- A process that blocks before its quantum expires keeps its level.
- A periodic boost moves ready processes to level 0 and resets their quantum
  state.

### Deadlock detection

Use the multi-instance detection algorithm:

1. Copy each resource's `available` vector into `work`.
2. Mark processes with no outstanding request as finishable.
3. Repeatedly find an unfinished process whose request is no greater than
   `work`, then add its allocation to `work`.
4. Unfinished processes at the end form the deadlocked set.

Detection must not mutate `allocated`, `requested`, `available`, or process
state. If a deadlock is found, print one deterministic line with process IDs
in ascending order:

```text
DEADLOCK t=<now> pids: <pid> <pid> ...
```

## Implementation order

1. Add shared scheduler helper functions and initialize each policy's queues.
2. Implement SJF and SRTF, then verify burst ordering and preemption.
3. Implement priority scheduling and equal-priority round robin.
4. Implement MLFQ demotion, blocking behavior, and periodic boosting.
5. Implement deadlock detection using copied work/finish arrays.
6. Add hand-computed expected metrics and Gantt-chart checks to
   `tests/run.sh`.
7. Run the complete regression script and record results.

## Validation plan

The existing workloads provide the first regression layer:

```sh
make
make test
```

For scheduler comparisons, use `workloads/arrivals.txt` with
`--no-paging --gantt`. Record the first dispatch, completion, waiting time,
and context-switch behavior for each policy. For synchronization, use
`workloads/deadlock.txt` with `--deadlock-every 5`; processes 1 and 2 must be
reported while process 3 completes.

Before implementing policy code, expected values should be written beside
each new test and checked against a manually drawn timeline. A test that
only checks successful termination is not sufficient for a scheduler.

## Out of scope for this preliminary setup

- No scheduler algorithm is completed here.
- No deadlock victim termination or recovery policy is selected.
- No changes are made to paging or disk modules.
- No new dependency or build-system change is required.
