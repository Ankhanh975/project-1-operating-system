# ossim

The starting code of the COMP2040 group project: a small simulator in which
processes, CPU scheduling, paging, a disk and semaphores run together, one
tick at a time. The tasks are in the project handout on Canvas. This file is
only about the code.

## Build and run

```
make
make test
./ossim --help
./ossim workloads/basic.txt --sched rr --quantum 4 --no-paging --gantt
```

`--trace` prints every change of state, `--gantt` the CPU timeline, and
`--csv out.csv` the numbers of each process.

## Where you work

| File | What to write |
|---|---|
| `src/sched.c` | SJF, SRTF, PRIO, MLFQ (FCFS and RR are given) |
| `src/sync.c` | `deadlock_check()` |
| `src/mem.c` | LRU and Clock (FIFO is given) |
| `src/disk.c` | SSTF, SCAN and C-LOOK (FCFS is given) |
| `tests/run.sh` | a check for every policy you write |
| `workloads/` | your own workloads |

Read `src/kernel.c` first, the tick loop is there. You normally do not change
the other files. The workload format is in `include/workload.h`.

`make test` prints SKIP for a policy that is not written yet, and PASS or
FAIL once it is.
