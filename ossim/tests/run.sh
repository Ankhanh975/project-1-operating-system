#!/bin/sh
# Tests against hand-computed textbook and checkpoint numbers.
#
# A check whose command stops with "not implemented yet" prints SKIP: the
# expected value is already here, waiting for your implementation. Every SKIP
# must have become PASS by the deadline of its checkpoint.
BIN=./ossim
fail=0; pass=0; skip=0

check() {   # check <description> <command> <expected-word>
    out=$($2 2>&1) || true
    if echo "$out" | grep -q "not implemented yet"; then
        echo "SKIP  $1"; skip=$((skip + 1))
    elif echo "$out" | grep -qw -- "$3"; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; echo "      expected: $3"; echo "$out" | sed 's/^/      /'; fail=1
    fi
}

check_contains() {   # check_contains <description> <command> <expected-text>
    out=$($2 2>&1) || true
    if echo "$out" | grep -q "not implemented yet"; then
        echo "SKIP  $1"; skip=$((skip + 1))
    elif echo "$out" | grep -Fq -- "$3"; then
        echo "PASS  $1"
        pass=$((pass + 1))
    else
        echo "FAIL  $1"; echo "      expected: $3"; echo "$out" | sed 's/^/      /'; fail=1
    fi
}

# --- reference policies (must pass from day one) --------------------------
check "FCFS textbook example: avg waiting 17.00" \
      "$BIN workloads/basic.txt --sched fcfs --no-paging" "avg_waiting=17.00"

check "RR q=4 textbook example: avg waiting 5.67" \
      "$BIN workloads/basic.txt --sched rr --quantum 4 --no-paging" "avg_waiting=5.67"

check "RR q=4: context switches = 4 (P1,P2,P3,P1)" \
      "$BIN workloads/basic.txt --sched rr --quantum 4 --no-paging" "context_switches=4"

check "Paging: P1 alone, FIFO, 3 frames -> 15 faults (textbook Sec. 10.4.2)" \
      "$BIN workloads/paging_p1.txt --frames 3 --replace fifo" "page_faults=15"

check "Disk: FCFS, head 53 -> 640 cylinders (textbook Sec. 11.2.1)" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --disk fcfs" "head_movement=640"

check "Producer/consumer completes" \
      "$BIN workloads/producer_consumer.txt --sched rr --quantum 2 --no-paging" "finished=2/2"

check "Deadlock workload without detection: A and B stall, C finishes" \
      "$BIN workloads/deadlock.txt --sched rr --quantum 2 --max-ticks 60 --no-paging" "finished=1/3"

check "Mixed workload finishes" \
      "$BIN workloads/mixed.txt --sched rr --quantum 3 --frames 6" "finished=4/4"

check "Mixed workload finishes with 1 frame (faults wait for a frame)" \
      "$BIN workloads/mixed.txt --sched rr --quantum 3 --frames 1" "finished=4/4"

# --- checkpoint 1 ----------------------------------------------------------
check "Deadlock detection reports A and B (pids 1 2)" \
      "$BIN workloads/deadlock.txt --sched rr --quantum 2 --deadlock-every 5 --max-ticks 60 --no-paging" "pids: 1 2"

check "Multi-instance deadlock reports A and B" \
      "$BIN workloads/deadlock_multi.txt --deadlock-every 1 --max-ticks 10 --no-paging" "pids: 1 2"

# Hand-computed results for arrivals.txt; compare waiting/response, dispatch
# behavior, and Gantt segments rather than merely checking termination.
check "FCFS arrivals: waiting 14.83, response 10.17" \
      "$BIN workloads/arrivals.txt --sched fcfs --no-paging --gantt" "avg_waiting=14.83 avg_response=10.17"
check_contains "FCFS arrivals: starts J1 and runs through t=6" \
      "$BIN workloads/arrivals.txt --sched fcfs --no-paging --gantt" "[0-6] P1"

check "RR q=2 arrivals: waiting 18.33, response 3.17" \
      "$BIN workloads/arrivals.txt --sched rr --quantum 2 --no-paging --gantt" "avg_waiting=18.33 avg_response=3.17"
check_contains "RR q=2 arrivals: first slice is J1" \
      "$BIN workloads/arrivals.txt --sched rr --quantum 2 --no-paging --gantt" "[0-1] P1"

check "SJF arrivals: waiting 10.00, response 9.00" \
      "$BIN workloads/arrivals.txt --sched sjf --no-paging --gantt" "avg_waiting=10.00 avg_response=9.00"
check_contains "SJF arrivals: first job runs non-preemptively" \
      "$BIN workloads/arrivals.txt --sched sjf --no-paging --gantt" "[0-6] P1"

check "SRTF arrivals: waiting 9.83, response 9.00" \
      "$BIN workloads/arrivals.txt --sched srtf --no-paging --gantt" "avg_waiting=9.83 avg_response=9.00"
check_contains "SRTF arrivals: equal remaining time does not preempt J1" \
      "$BIN workloads/arrivals.txt --sched srtf --no-paging --gantt" "[0-6] P1"
check_contains "SRTF arrivals: J3 preempts J5 when strictly shorter" \
      "$BIN workloads/arrivals.txt --sched srtf --no-paging --gantt" "[16-16] P5 | [17-19] P3"

check "PRIO arrivals: waiting 12.83, response 8.33" \
      "$BIN workloads/arrivals.txt --sched prio --quantum 2 --no-paging --gantt" "avg_waiting=12.83 avg_response=8.33"
check_contains "PRIO arrivals: highest-priority J2 preempts J1" \
      "$BIN workloads/arrivals.txt --sched prio --quantum 2 --no-paging --gantt" "[0-1] P1"

check "MLFQ arrivals: waiting 15.00, response 0.00" \
      "$BIN workloads/arrivals.txt --sched mlfq --quantum 2 --no-paging --gantt" "avg_waiting=15.00 avg_response=0.00"
check_contains "MLFQ arrivals: starts with J1 at level 0" \
      "$BIN workloads/arrivals.txt --sched mlfq --quantum 2 --no-paging --gantt" "[0-1] P1"
check_contains "MLFQ arrivals: tick-20 boost resets the running quantum" \
      "$BIN workloads/arrivals.txt --sched mlfq --quantum 2 --no-paging --gantt" "[19-21] P5 | [22-23] P1"

check "PRIO q=2 rotates equal priorities" \
      "$BIN workloads/prio_ties.txt --sched prio --quantum 2 --no-paging --gantt" "avg_waiting=4.50 avg_response=1.00"
check_contains "PRIO q=2 tie order is round robin" \
      "$BIN workloads/prio_ties.txt --sched prio --quantum 2 --no-paging --gantt" "[0-1] P1 | [2-3] P2 | [4-5] P1"

for s in sjf srtf prio mlfq; do
    check "Producer/consumer completes under $s" \
          "$BIN workloads/producer_consumer.txt --sched $s --quantum 2 --no-paging" "finished=2/2"
done

# Textbook cases (Sec. 5.3.2). The Gantt charts are in the workload files.
check "SJF textbook example: avg waiting 7.00" \
      "$BIN workloads/sjf_textbook.txt --sched sjf --no-paging" "avg_waiting=7.00"
check "SRTF textbook example: avg waiting 6.50" \
      "$BIN workloads/srtf_textbook.txt --sched srtf --no-paging" "avg_waiting=6.50"
check "SRTF textbook example: context switches = 5 (P1,P2,P4,P1,P3)" \
      "$BIN workloads/srtf_textbook.txt --sched srtf --no-paging" "context_switches=5"

# Context switches on arrivals.txt, counted on the hand-drawn Gantt charts.
check "SJF arrivals: context switches = 8" \
      "$BIN workloads/arrivals.txt --sched sjf --no-paging" "context_switches=8"
check "SRTF arrivals: context switches = 9" \
      "$BIN workloads/arrivals.txt --sched srtf --no-paging" "context_switches=9"

# --- checkpoint 2 ----------------------------------------------------------
check "Paging: LRU, 3 frames -> 12 faults (textbook Sec. 10.4.4)" \
      "$BIN workloads/paging_p1.txt --frames 3 --replace lru" "page_faults=12"
# Clock on the same string, traced by hand (the textbook has no Clock
# number). The frame-by-frame trace is in CHECKPOINT2.md.
check "Paging: Clock, 3 frames -> 14 faults (hand trace)" \
      "$BIN workloads/paging_p1.txt --frames 3 --replace clock" "page_faults=14"

check "Disk: SSTF, head 53 -> 236" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --disk sstf" "head_movement=236"
check "Disk: SCAN moving down, head 53 -> 236" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction down --disk scan" "head_movement=236"
check "Disk: C-LOOK moving up, head 53 -> 322" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction up --disk clook" "head_movement=322"
# The other direction of each, computed by hand:
#   SCAN up:     53 -> 183 -> 199 (edge) -> 14     = 146 + 185      = 331
#   C-LOOK down: 53 -> 14 -> 183 (jump) -> 65      = 39 + 169 + 118 = 326
check "Disk: SCAN moving up, head 53 -> 331" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction up --disk scan" "head_movement=331"
check "Disk: C-LOOK moving down, head 53 -> 326" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction down --disk clook" "head_movement=326"

# Workload generator: the same seed gives the same file, and the file runs.
GEN=./wlgen
if [ -x "$GEN" ]; then
    gen1=$($GEN --procs 6 --seed 42 --disk-local 50)
    gen2=$($GEN --procs 6 --seed 42 --disk-local 50)
    if [ "$gen1" = "$gen2" ]; then
        echo "PASS  Generator: same seed gives the same workload"; pass=$((pass + 1))
    else
        echo "FAIL  Generator: same seed gives different workloads"; fail=1
    fi
    tmp=${TMPDIR:-/tmp}/ossim_gen_$$.txt
    echo "$gen1" > "$tmp"
    check "Generator: generated workload finishes (6 procs, 8 frames)" \
          "$BIN $tmp --sched rr --quantum 3 --frames 8 --replace clock --disk scan" "finished=6/6"
    rm -f "$tmp"
else
    echo "SKIP  Generator: ./wlgen not built (run make)"; skip=$((skip + 1))
fi

echo "---"
echo "$pass passed, $skip skipped (not implemented yet), $([ $fail = 0 ] && echo 0 || echo some) failed"
exit $fail
