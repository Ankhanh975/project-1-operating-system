#!/bin/sh
# Experiment 1: waiting and response time of the six CPU schedulers as the
# I/O intensity of the workload grows.
#
#   sh experiments/exp1_io.sh [output-dir]      (run from ossim/, after make)
#
# For every I/O intensity P (percentage of gaps between CPU bursts that block
# on io or disk) and every seed, wlgen makes one workload, and every
# scheduler runs it with --no-paging, so the results depend on the CPU
# scheduler only. Each value in the CSV is the average over the seeds.
#
# Output (one row per P, one column per scheduler):
#   exp1_waiting.csv    average waiting time
#   exp1_response.csv   average response time

OUT=${1:-../report/data}
SEEDS="1 2 3 4 5 6 7 8 9 10"
IO_LEVELS="0 25 50 75 100"
SCHEDS="fcfs rr sjf srtf prio mlfq"
QUANTUM=4
GEN_OPTS="--procs 10 --gap 3 --bursts 4 --cpu 6 --io-len 6 --disk 50"

[ -x ./ossim ] && [ -x ./wlgen ] || { echo "run make first" >&2; exit 1; }
mkdir -p "$OUT"
tmp=${TMPDIR:-/tmp}/ossim_exp1_$$.txt
trap 'rm -f "$tmp"' EXIT

# Value of key=<number> in the summary of an ossim run.
metric() { sed -n "s/.*$1=\([0-9.]*\).*/\1/p"; }

header="io"
for s in $SCHEDS; do header="$header,$s"; done
echo "$header" > "$OUT/exp1_waiting.csv"
echo "$header" > "$OUT/exp1_response.csv"

for io in $IO_LEVELS; do
    wrow="$io"; rrow="$io"
    for s in $SCHEDS; do
        wsum=0; rsum=0; n=0
        for seed in $SEEDS; do
            ./wlgen $GEN_OPTS --io "$io" --seed "$seed" > "$tmp"
            out=$(./ossim "$tmp" --sched "$s" --quantum $QUANTUM --no-paging)
            w=$(echo "$out" | metric avg_waiting)
            r=$(echo "$out" | metric avg_response)
            wsum=$(echo "$wsum $w" | awk '{print $1 + $2}')
            rsum=$(echo "$rsum $r" | awk '{print $1 + $2}')
            n=$((n + 1))
        done
        wrow="$wrow,$(echo "$wsum $n" | awk '{printf "%.2f", $1 / $2}')"
        rrow="$rrow,$(echo "$rsum $n" | awk '{printf "%.2f", $1 / $2}')"
    done
    echo "$wrow" >> "$OUT/exp1_waiting.csv"
    echo "$rrow" >> "$OUT/exp1_response.csv"
    echo "io=$io done" >&2
done

echo "wrote $OUT/exp1_waiting.csv and $OUT/exp1_response.csv" >&2
