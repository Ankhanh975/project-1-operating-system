# ossim

The COMP2040 group project: a small simulator in which processes, CPU
scheduling, paging, a disk and semaphores run together, one tick at a time.
The tasks are in the project handout on Canvas. This file is the
implementation overview; the design notes, with every hand-computed case,
are [`CHECKPOINT1.md`](CHECKPOINT1.md) (processes, scheduling,
synchronization) and [`CHECKPOINT2.md`](CHECKPOINT2.md) (memory, disk,
workload generator).

## Build and run

```
make
make test
./ossim --help
./ossim workloads/basic.txt --sched rr --quantum 4 --no-paging --gantt
./wlgen --procs 10 --seed 7 > /tmp/w.txt && ./ossim /tmp/w.txt --frames 16 --replace lru
```

`--trace` prints every change of state, `--gantt` the CPU timeline, and
`--csv out.csv` the numbers of each process.

`make` builds two programs: `ossim`, the simulator, and `wlgen`, the
workload generator. Both must build with `-std=c99 -Wall -Wextra -Werror`
and no external libraries. On Windows, build and test inside WSL.

## Status

| Phase | Content | Status |
|---|---|---|
| Scaffold | FCFS, RR, FIFO replacement, FCFS disk, semaphores, resources | given |
| Checkpoint 1 | SJF, SRTF, PRIO (RR ties), MLFQ (`q`, `2q`, `4q`, boost), multi-instance deadlock detection | done |
| Checkpoint 2 | LRU, Clock, SSTF, SCAN, C-LOOK, workload generator | done |
| Theme | one extension behind a command-line option | pending |

`make test` currently reports 45 PASS, 0 SKIP and 0 FAIL.

## Policies

| Option | Values |
|---|---|
| `--sched` | `fcfs`, `rr`, `sjf`, `srtf`, `prio`, `mlfq` |
| `--replace` | `fifo`, `lru`, `clock` |
| `--disk` | `fcfs`, `sstf`, `scan`, `clook` |

## Project structure

| File | Contents |
|---|---|
| `include/sim.h` | settings, the clock and the limits |
| `include/process.h` | PCB fields shared by all schedulers and synchronization |
| `include/queue.h` | FIFO ready/waiting queues and min-key operations |
| `include/sched.h`, `src/sched.c` | scheduler hook interface and the six CPU schedulers |
| `include/sync.h`, `src/sync.c` | semaphores, resources and deadlock detection |
| `include/mem.h`, `src/mem.c` | paging and the three replacement policies |
| `include/disk.h`, `src/disk.c` | the disk and the four disk schedulers |
| `src/kernel.c` | tick loop and lifecycle integration; change only when needed |
| `src/metrics.c` | report, Gantt chart and CSV output |
| `tools/wlgen.c` | workload generator (separate program) |
| `experiments/` | scripts of the report experiments; they write CSV files to `../report/data/` |
| `tests/run.sh` | regression and hand-computed checkpoint tests |
| `workloads/` | deterministic test workloads (see below) |
| `CHECKPOINT1.md`, `CHECKPOINT2.md` | design notes: rules, invariants, hand traces and results |

Read `src/kernel.c` first because it owns the tick loop and calls the policy
hooks. The workload format is in `include/workload.h`. Keep the existing
`scheduler_t`, `replacer_t` and `disk_sched_t` interfaces stable so the
kernel remains policy independent.

## Workloads

| File | Used for |
|---|---|
| `basic.txt` | FCFS and RR textbook example (Sec. 5.3.1) |
| `sjf_textbook.txt` | SJF textbook example (Sec. 5.3.2), avg waiting 7.00 |
| `srtf_textbook.txt` | SRTF textbook example (Sec. 5.3.2), avg waiting 6.50 |
| `arrivals.txt` | comparison of all schedulers with arrivals, priorities and I/O |
| `producer_consumer.txt` | bounded buffer with semaphores |
| `deadlock.txt` | two-resource deadlock for detection |
| `deadlock_multi.txt` | deadlock with two instances per resource (multi-instance detection) |
| `prio_ties.txt` | equal priorities sharing the CPU in round robin |
| `mixed.txt` | CPU, I/O, disk and paging together |
| `paging_p1.txt` | textbook reference string: FIFO 15, LRU 12, Clock 14 faults |
| `paging.txt` | two processes competing for global frames |
| `disk_textbook.txt` | textbook disk queue (Sec. 11.2), head 53 |

Larger workloads for the experiments come from `./wlgen` (options in
`CHECKPOINT2.md`).

## Tests

`make test` runs `tests/run.sh`. Every check compares one number of the
summary with a value computed by hand (textbook examples and our own traces).
A policy that is not written yet prints SKIP; once it exists, the check
prints PASS or FAIL.
