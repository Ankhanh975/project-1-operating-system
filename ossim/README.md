# ossim

The starting code of the COMP2040 group project: a small simulator in which
processes, CPU scheduling, paging, a disk and semaphores run together, one
tick at a time. The tasks are in the project handout on Canvas. This file is
the implementation overview; the Checkpoint 1 plan and design note are in
[`CHECKPOINT1.md`](CHECKPOINT1.md).

## Build and run

```
make
make test
./ossim --help
./ossim workloads/basic.txt --sched rr --quantum 4 --no-paging --gantt
```

`--trace` prints every change of state, `--gantt` the CPU timeline, and
`--csv out.csv` the numbers of each process.

## Current scope: Checkpoint 1

This first phase is intentionally limited to process scheduling and
synchronization:

- SJF (non-preemptive)
- SRTF
- preemptive priority scheduling with round-robin ties
- three-level MLFQ with quanta `q`, `2q`, and `4q`
- periodic MLFQ priority boosts
- multi-instance deadlock detection
- hand-computed workload tests and a short design note

Paging replacement, disk scheduling, workload generation, and the project
theme are later phases. The corresponding files remain scaffolded and should
not be implemented as part of this preliminary setup.

## Project structure

| File | What to write |
|---|---|
| `include/process.h` | PCB fields shared by all schedulers and synchronization |
| `include/queue.h` | FIFO ready/waiting queues and min-key operations |
| `include/sched.h` | scheduler hook interface |
| `src/sched.c` | Checkpoint 1 CPU scheduler implementations |
| `include/sync.h` | semaphore/resource state and detection inputs |
| `src/sync.c` | Checkpoint 1 deadlock detection |
| `src/kernel.c` | tick loop and lifecycle integration; change only when needed |
| `tests/run.sh` | regression and hand-computed checkpoint tests |
| `workloads/` | deterministic test workloads |
| `CHECKPOINT1.md` | implementation plan, invariants, and validation plan |

Read `src/kernel.c` first because it owns the tick loop and calls the scheduler
hooks. The workload format is in `include/workload.h`. Keep the existing
`scheduler_t` and synchronization APIs stable so the kernel remains policy
independent.

## Preliminary status

FCFS, RR, semaphore operations, resource allocation, and the simulator
integration are present in the scaffold. The Checkpoint 1 policies and
`deadlock_check()` still report that they are not implemented; this is
deliberate until the design and hand-computed expected values are reviewed.

`make test` prints SKIP for a policy that is not written yet, and PASS or
FAIL once it is.
