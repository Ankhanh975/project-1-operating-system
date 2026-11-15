# Checkpoint 1 implementation and design note

## Goal

Complete the process, scheduling, and synchronization portion of `ossim`
without changing the public simulator interface. The simulator remains
tick-driven: the kernel owns process state transitions and calls a selected
policy through `scheduler_t`.

Checkpoint 1 is implemented and validated. Scheduler policy state remains
inside the scheduler module, while process-state transitions remain owned by
the kernel.

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

## Validation and comparison

The regression suite covers hand-computed textbook values, scheduling
metrics/Gantt checks on `workloads/arrivals.txt`, priority round-robin ties,
the producer/consumer synchronization workload, and both single-instance
and multi-instance deadlocks. Run it with:

```sh
make
make test
```

All comparisons below use `workloads/arrivals.txt`, `--no-paging`, and
quantum 2 for RR, PRIO, and MLFQ. Times in the Gantt column are inclusive.
The first dispatch is J1 under every policy.

| Policy | Gantt chart (tick range: process) | Completion tick | Avg. waiting | Avg. response | Context switches |
|---|---|---:|---:|---:|---:|
| FCFS | 0-6:J1, 7-15:J2, 16-18:J3, 19-20:J4, 21-25:J5, 26-29:J1, 30-35:J6, 36-38:J3 | 39 | 14.83 | 10.17 | 8 |
| RR (q=2) | 0-1:J1, 2-3:J2, 4-5:J1, 6-7:J3, 8-9:J2, 10-11:J4, 12-13:J1, 14-15:J5, 16:J3, 17-18:J2, 19-20:J6, 21:J1, 22-23:J5, 24-25:J2, 26-27:J6, 28-29:J3, 30:J5, 31-32:J1, 33:J2, 34-35:J6, 36:J3, 37-38:J1 | 39 | 18.33 | 3.17 | 22 |
| SJF | 0-6:J1, 7-8:J4, 9-11:J3, 12-15:J1, 16-20:J5, 21-23:J3, 24-29:J6, 30-38:J2 | 39 | 10.00 | 9.00 | 8 |
| SRTF | 0-6:J1, 7-8:J4, 9-11:J3, 12-15:J1, 16:J5, 17-19:J3, 20-23:J5, 24-29:J6, 30-38:J2 | 39 | 9.83 | 9.00 | 9 |
| PRIO (q=2) | 0-1:J1, 2-10:J2, 11:J1, 12-17:J6, 18-19:J4, 20-23:J1, 24-25:J3, 26:J5, 27-30:J1, 31:J3, 32-35:J5, 36:idle, 37-39:J3 | 40 | 12.83 | 8.33 | 12 |
| MLFQ (q=2) | 0-1:J1, 2-3:J2, 4-5:J3, 6-7:J4, 8-9:J5, 10-11:J1, 12-13:J6, 14-17:J2, 18:J3, 19-21:J5, 22-23:J1, 24-25:J6, 26-27:J2, 28-30:J3, 31:J1, 32-33:J6, 34:J2, 35-38:J1 | 39 | 15.00 | 0.00 | 18 |

This workload has staggered arrivals, CPU/I/O bursts, and priorities.
SJF/SRTF select by the remaining current burst, not total process CPU time;
at tick 4 SRTF does not preempt J1 because J1 and J3 both have three ticks
left. It later preempts J5 at tick 17 when J3's remaining burst is strictly
shorter. The PRIO timeline shows J2 preempting J1; `prio_ties.txt` separately
checks round-robin rotation between equal-priority processes. MLFQ's timeline
spans the tick-20 boost, and its policy uses quanta 2, 4, and 8.

The deadlock detector copies Available into a work vector, iteratively
simulates finishable processes, and reports remaining blocked processes in
ascending PID order. `deadlock.txt` checks the classic one-instance case;
`deadlock_multi.txt` checks a deadlock where each process holds two instances
and requests one instance from the other resource. Detection is report-only:
it does not mutate resource allocations or recover from deadlock.

## Deferred beyond Checkpoint 1

- No deadlock victim termination or recovery policy is selected.
- Paging replacement and disk scheduling are deferred to later checkpoints.
