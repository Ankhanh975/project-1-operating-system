#!/bin/sh
# Experiment 2: page-fault rate and CPU utilization as the number of frames
# shrinks, for FIFO, LRU and Clock, and the onset of thrashing.
#
#   sh experiments/exp2_frames.sh [output-dir]  (run from ossim/, after make)
#
# The workloads have little I/O (--io 10) and long CPU bursts, so most of the
# blocking comes from page faults. Every run uses RR with q=4 and the FCFS
# disk, so only the memory size and the replacement policy change. A run
# that makes no progress is stopped by --max-ticks; it still counts, with
# the utilization and faults it reached. Each value is the average over the
# seeds.
#
# Output (one row per frame count, one column per policy):
#   exp2_faultrate.csv     page faults per executed reference
#   exp2_util.csv          CPU utilization in percent (user work only)
#   exp2_finished.csv      percentage of processes that finished

OUT=${1:-../report/data}
SEEDS="1 2 3 4 5 6 7 8 9 10"
FRAMES="128 96 64 48 32 24 16 12 8 6 4"
POLICIES="fifo lru clock"
GEN_OPTS="--procs 8 --pages 16 --locality 70 --io 10 --cpu 30 --bursts 4 --gap 2"
SIM_OPTS="--sched rr --quantum 4 --disk fcfs --max-ticks 20000"

[ -x ./ossim ] && [ -x ./wlgen ] || { echo "run make first" >&2; exit 1; }
mkdir -p "$OUT"
tmpdir=${TMPDIR:-/tmp}/ossim_exp2_$$
mkdir -p "$tmpdir"
trap 'rm -rf "$tmpdir"' EXIT

# The workloads do not depend on the frame count: generate them once.
for seed in $SEEDS; do
    ./wlgen $GEN_OPTS --seed "$seed" > "$tmpdir/w$seed.txt"
done

# Value of key=<number> in the summary of an ossim run.
metric() { sed -n "s/.*$1=\([0-9.]*\).*/\1/p"; }

header="frames"
for p in $POLICIES; do header="$header,$p"; done
for f in faultrate util finished; do echo "$header" > "$OUT/exp2_$f.csv"; done

for frames in $FRAMES; do
    frow="$frames"; urow="$frames"; drow="$frames"
    for p in $POLICIES; do
        fsum=0; usum=0; dsum=0; n=0
        for seed in $SEEDS; do
            out=$(./ossim "$tmpdir/w$seed.txt" $SIM_OPTS --frames "$frames" --replace "$p")
            fr=$(echo "$out" | metric fault_rate)
            ut=$(echo "$out" | metric utilization)
            # finished=<done>/<total> -> percentage
            fin=$(echo "$out" | sed -n 's/.*finished=\([0-9]*\)\/\([0-9]*\).*/\1 \2/p' \
                  | awk '{printf "%.2f", 100 * $1 / $2}')
            fsum=$(echo "$fsum $fr"  | awk '{print $1 + $2}')
            usum=$(echo "$usum $ut"  | awk '{print $1 + $2}')
            dsum=$(echo "$dsum $fin" | awk '{print $1 + $2}')
            n=$((n + 1))
        done
        frow="$frow,$(echo "$fsum $n" | awk '{printf "%.4f", $1 / $2}')"
        urow="$urow,$(echo "$usum $n" | awk '{printf "%.2f", $1 / $2}')"
        drow="$drow,$(echo "$dsum $n" | awk '{printf "%.1f", $1 / $2}')"
    done
    echo "$frow" >> "$OUT/exp2_faultrate.csv"
    echo "$urow" >> "$OUT/exp2_util.csv"
    echo "$drow" >> "$OUT/exp2_finished.csv"
    echo "frames=$frames done" >&2
done

echo "wrote $OUT/exp2_faultrate.csv, exp2_util.csv and exp2_finished.csv" >&2
