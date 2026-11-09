#!/bin/sh
# Tests against hand-computed textbook numbers. Extend this file as you
# implement policies (e.g. SJF/SRTF/PRIO/MLFQ on arrivals.txt, your own
# paging and disk workloads).
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

for s in sjf srtf prio mlfq; do
    check "Producer/consumer completes under $s" \
          "$BIN workloads/producer_consumer.txt --sched $s --quantum 2 --no-paging" "finished=2/2"
done
# TODO: add hand-computed avg_waiting / avg_response for every scheduler on
# workloads/arrivals.txt (and show the Gantt chart in your design note).

# --- checkpoint 2 ----------------------------------------------------------
check "Paging: LRU, 3 frames -> 12 faults (textbook Sec. 10.4.4)" \
      "$BIN workloads/paging_p1.txt --frames 3 --replace lru" "page_faults=12"
# TODO: a Clock case you computed by hand (the textbook has none, make one).

check "Disk: SSTF, head 53 -> 236" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --disk sstf" "head_movement=236"
check "Disk: SCAN moving down, head 53 -> 236" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction down --disk scan" "head_movement=236"
check "Disk: C-LOOK moving up, head 53 -> 322" \
      "$BIN workloads/disk_textbook.txt --no-paging --head 53 --direction up --disk clook" "head_movement=322"

echo "---"
echo "$pass passed, $skip skipped (not implemented yet), $([ $fail = 0 ] && echo 0 || echo some) failed"
exit $fail
