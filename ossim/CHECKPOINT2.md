# Checkpoint 2 design note

## Goal

Complete the memory and storage portion of `ossim`: two page-replacement
policies, three disk schedulers and a workload generator, each verified
against hand-computed cases. The public interfaces (`replacer_t`,
`disk_sched_t`) and the kernel are unchanged.

## Status

| Item | Where | Status |
|---|---|---|
| LRU replacement | `src/mem.c` | done, tested |
| Clock (second chance) replacement | `src/mem.c` | done, tested |
| SSTF disk scheduling | `src/disk.c` | done, tested |
| SCAN disk scheduling | `src/disk.c` | done, tested |
| C-LOOK disk scheduling | `src/disk.c` | done, tested |
| Workload generator | `tools/wlgen.c` | done, tested |
| Tests | `tests/run.sh` | 45 PASS, 0 SKIP, 0 FAIL |

## Page replacement

Replacement is global: all processes share `cfg.frames` frames. The kernel
already keeps the per-frame bookkeeping for every policy, so a policy only
has to choose a victim:

- `mem_access()` sets `last_used = now` and `referenced = true` on a hit.
- `mem_install()` sets both when a page-in finishes.
- A frame that is still loading (`loading == true`) can never be a victim.

### LRU

- Victim: the evictable frame with the smallest `last_used`.
- Ties go to the lower frame index (deterministic runs).
- `on_hit` and `on_install` do nothing; the timestamps are already set.

### Clock

- A `hand` stays between faults and moves round the frame table.
- Free or loading frame: skipped. `referenced == true`: cleared, the page
  gets its second chance. `referenced == false`: victim.
- The hand stops one frame past the victim.
- At most two turns are needed: the first clears every bit, the second finds
  a victim. `mem.h` guarantees at least one evictable frame.

### Hand trace: Clock, 3 frames, `workloads/paging_p1.txt`

Reference string `7,0,1,2,0,3,0,4,2,3,0,3,0,3,2,1,2,0,1,7,0,1`. `*` marks a
set reference bit; `h` is the frame the hand points at after the reference.

| # | Ref | F0 | F1 | F2 | Fault | h |
|---|---|---|---|---|---|---|
| 1 | 7 | 7* | - | - | yes | 0 |
| 2 | 0 | 7* | 0* | - | yes | 0 |
| 3 | 1 | 7* | 0* | 1* | yes | 0 |
| 4 | 2 | 2* | 0 | 1 | yes | 1 |
| 5 | 0 | 2* | 0* | 1 | | 1 |
| 6 | 3 | 2* | 0 | 3* | yes | 0 |
| 7 | 0 | 2* | 0* | 3* | | 0 |
| 8 | 4 | 4* | 0 | 3 | yes | 1 |
| 9 | 2 | 4* | 2* | 3 | yes | 2 |
| 10 | 3 | 4* | 2* | 3* | | 2 |
| 11 | 0 | 4 | 2 | 0* | yes | 0 |
| 12 | 3 | 3* | 2 | 0* | yes | 1 |
| 13 | 0 | 3* | 2 | 0* | | 1 |
| 14 | 3 | 3* | 2 | 0* | | 1 |
| 15 | 2 | 3* | 2* | 0* | | 1 |
| 16 | 1 | 3 | 1* | 0 | yes | 2 |
| 17 | 2 | 3 | 1* | 2* | yes | 0 |
| 18 | 0 | 0* | 1* | 2* | yes | 1 |
| 19 | 1 | 0* | 1* | 2* | | 1 |
| 20 | 7 | 0 | 7* | 2 | yes | 2 |
| 21 | 0 | 0* | 7* | 2 | | 2 |
| 22 | 1 | 0* | 7* | 1* | yes | 0 |

Total: **14 faults**, between LRU (12) and FIFO (15). Clock approximates LRU
with one bit per frame, so it can only tell "used since the last sweep" from
"not used", which costs two extra faults on this string.

## Disk scheduling

A policy is a `pick()` over the pending requests. It returns the index of
the next request and may change the direction. SCAN reports the trip to the
edge in `via[0]`, so `disk_kick()` counts that movement.

`nearest_in_direction()` is shared by SCAN and C-LOOK: the nearest request
ahead of the head in the current direction (a request on the head's own
cylinder counts for both directions). Equal distances go to the older
request.

### SSTF

- The request with the smallest `|cylinder - head|`, ties to the older one.
- Can starve a far request while new ones keep arriving near the head
  (Sec. 11.2.2). See "Observed behavior" below.

### SCAN

- Nearest request in the current direction.
- If none is left that way: go to the edge (`0` or `cylinders - 1`), turn
  around, serve the nearest request from the edge.

### C-LOOK

- Nearest request in the current direction, no trip to the edge.
- If none is left that way: jump to the furthest request the other way
  (the lowest when moving up), keep the direction. The jump counts as head
  movement.

### Hand-computed cases: `workloads/disk_textbook.txt`, head 53, 200 cylinders

Queue: 98, 183, 37, 122, 14, 124, 65, 67.

| Policy | Order served | Movement |
|---|---|---|
| FCFS | 98, 183, 37, 122, 14, 124, 65, 67 | 640 |
| SSTF | 65, 67, 37, 14, 98, 122, 124, 183 | 236 |
| SCAN down | 37, 14, (0), 65, 67, 98, 122, 124, 183 | 53 + 183 = 236 |
| SCAN up | 65, 67, 98, 122, 124, 183, (199), 37, 14 | 146 + 185 = 331 |
| C-LOOK up | 65, 67, 98, 122, 124, 183, 14, 37 | 130 + 169 + 23 = 322 |
| C-LOOK down | 37, 14, 183, 124, 122, 98, 67, 65 | 39 + 169 + 118 = 326 |

The first four rows are the textbook numbers. SCAN up and C-LOOK down are
our own traces.

## Workload generator

`tools/wlgen.c` is built by `make` as `./wlgen`. It writes a workload to
stdout in the format of `include/workload.h`:

```sh
./wlgen --procs 10 --seed 7 --io 60 --locality 50 > /tmp/w.txt
./ossim /tmp/w.txt --sched rr --quantum 3 --frames 16 --replace lru --disk scan
```

| Option | Meaning | Experiment it serves |
|---|---|---|
| `--procs`, `--gap` | number of processes, mean gap between arrivals | load |
| `--bursts`, `--cpu` | cpu bursts per process, mean burst length | CPU demand |
| `--io`, `--disk`, `--io-len` | % of gaps that block, % of those on the disk, io length | I/O intensity vs schedulers |
| `--pages`, `--locality` | address space, % of references within one page of the last | page faults vs frames, thrashing |
| `--cylinders`, `--disk-local` | disk size, % of requests within 5% of the last | random vs localized disk requests |
| `--prios` | priorities 1..N | PRIO and MLFQ |

The generator has its own 64-bit LCG instead of `rand()`, so the same
`--seed` gives the same file on Linux, macOS and WSL. The first line of the
file records the command line that produced it. `tests/run.sh` checks that
two runs with the same seed are identical and that a generated workload
finishes.

## Observed behavior: thrashing livelock

Running every policy combination on generated workloads (12 processes,
`--locality 50`, 8 seeds, 1728 runs) finished every run with 40 frames.
With 2 or 6 frames, 86 runs made almost no progress until `--max-ticks`
(for example 7 references served and ~50 000 page faults).

The cause is in the kernel, not in a policy: a page that has just been
loaded is not protected until its owner runs. With more ready processes
than frames, every page-in evicts a page whose owner is still waiting in
the ready queue, so each process faults again as soon as it is dispatched.
It happens with the given FIFO replacement and FCFS disk too. SSTF makes it
much more frequent (80 of the 86 runs): page-ins far from the head starve.

This is thrashing in its extreme form. It is a candidate for the
"onset of thrashing" experiment and is exactly what theme 1 (working-set
load control) is meant to prevent.

## Theme and plan

> To be completed by the group: chosen theme (from the list in the
> project brief or an agreed one), the command-line option that turns it on,
> the hand-computed test, and the experiment question.

## Out of scope

- Changes to the kernel's page-fault path (the livelock above is kept as
  observed behavior).
- C-SCAN and LOOK, which the brief does not ask for.
