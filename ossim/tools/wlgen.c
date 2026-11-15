/*
 * wlgen.c: random workload generator for ossim (checkpoint 2).
 *
 * Writes a workload file in the format of include/workload.h to stdout:
 *
 *   ./wlgen --procs 10 --seed 7 --io 60 > workloads/gen.txt
 *   ./ossim workloads/gen.txt --sched rr --frames 16 --replace lru
 *
 * Every process is a chain of cpu bursts. Between two bursts the process
 * does one of: generic io, a disk request, or nothing (the bursts merge).
 * The knobs map to the experiments of the report:
 *
 *   --io P          percentage of gaps that are io or disk (I/O intensity)
 *   --disk P        of those, the percentage that go to the disk
 *   --locality P    percentage of page references that stay near the last
 *                   one (high = good locality, low = thrashing candidate)
 *   --disk-local P  percentage of disk requests near the previous cylinder
 *                   of the same process (localized vs random disk traffic)
 *
 * The generator has its own random number generator (a 64-bit LCG), so the
 * same --seed gives the same file on every platform. That matters for the
 * report: every experiment can be repeated exactly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Limits of the simulator (include/sim.h). Kept in sync by hand because the
 * generator is a separate program and does not link with ossim. */
#define MAX_PROCS 64
#define MAX_OPS   128
#define MAX_REFS  512
#define MAX_PAGES 1024

typedef struct options {
    int      procs;        /* number of processes */
    unsigned long long seed;
    int      gap;          /* mean ticks between two arrivals */
    int      bursts;       /* cpu bursts per process */
    int      cpu;          /* mean length of a cpu burst */
    int      io;           /* % of gaps between bursts that block */
    int      disk;         /* % of blocking gaps that are disk requests */
    int      io_len;       /* mean length of a generic io */
    int      pages;        /* virtual pages per process */
    int      locality;     /* % of references near the previous one */
    int      cylinders;    /* disk size, must match ossim --cylinders */
    int      disk_local;   /* % of disk requests near the previous one */
    int      prios;        /* priorities are 1..prios */
} options_t;

/* Random numbers */
static unsigned long long rng_state;

static void rng_seed(unsigned long long s) { rng_state = s * 2862933555777941757ULL + 3037000493ULL; }

/* Uniform integer in [lo, hi]. Uses the high bits of the LCG, which are the
 * most random ones. */
static int rnd(int lo, int hi)
{
    rng_state = rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    unsigned long long r = rng_state >> 33;
    return lo + (int)(r % (unsigned long long)(hi - lo + 1));
}

/* True with probability pct percent. */
static int chance(int pct) { return rnd(1, 100) <= pct; }

/* Value around mean: uniform in [1, 2*mean - 1], so the mean is `mean`. */
static int around(int mean) { return mean <= 1 ? 1 : rnd(1, 2 * mean - 1); }

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

/* Command line */
static void usage(void)
{
    fprintf(stderr,
        "usage: wlgen [options] > workload.txt\n"
        "  --procs N        number of processes                  (default 8, max %d)\n"
        "  --seed S         random seed                          (default 1)\n"
        "  --gap N          mean ticks between arrivals          (default 3)\n"
        "  --bursts N       cpu bursts per process               (default 3)\n"
        "  --cpu N          mean cpu burst length                (default 6)\n"
        "  --io P           %% of gaps that block on io or disk   (default 50)\n"
        "  --disk P         %% of blocking gaps that use the disk (default 50)\n"
        "  --io-len N       mean generic io length               (default 4)\n"
        "  --pages N        virtual pages per process            (default 8)\n"
        "  --locality P     %% of references near the last one    (default 80)\n"
        "  --cylinders N    disk cylinders, as in ossim          (default 200)\n"
        "  --disk-local P   %% of disk requests near the last one (default 0)\n"
        "  --prios N        priorities 1..N                      (default 3)\n",
        MAX_PROCS);
    exit(1);
}

static int need_int(int argc, char **argv, int *i, int lo, int hi)
{
    if (*i + 1 >= argc) usage();
    char *end;
    long v = strtol(argv[++*i], &end, 10);
    if (*end || v < lo || v > hi) {
        fprintf(stderr, "wlgen: %s must be in %d..%d\n", argv[*i - 1], lo, hi);
        exit(1);
    }
    return (int)v;
}

static options_t parse(int argc, char **argv)
{
    options_t o = { .procs = 8, .seed = 1, .gap = 3, .bursts = 3, .cpu = 6,
                    .io = 50, .disk = 50, .io_len = 4, .pages = 8, .locality = 80,
                    .cylinders = 200, .disk_local = 0, .prios = 3 };
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--procs"))      o.procs      = need_int(argc, argv, &i, 1, MAX_PROCS);
        else if (!strcmp(a, "--seed"))       o.seed       = (unsigned long long)need_int(argc, argv, &i, 0, 2147483647);
        else if (!strcmp(a, "--gap"))        o.gap        = need_int(argc, argv, &i, 0, 10000);
        else if (!strcmp(a, "--bursts"))     o.bursts     = need_int(argc, argv, &i, 1, MAX_OPS / 2);
        else if (!strcmp(a, "--cpu"))        o.cpu        = need_int(argc, argv, &i, 1, MAX_REFS / 2);
        else if (!strcmp(a, "--io"))         o.io         = need_int(argc, argv, &i, 0, 100);
        else if (!strcmp(a, "--disk"))       o.disk       = need_int(argc, argv, &i, 0, 100);
        else if (!strcmp(a, "--io-len"))     o.io_len     = need_int(argc, argv, &i, 1, 10000);
        else if (!strcmp(a, "--pages"))      o.pages      = need_int(argc, argv, &i, 1, MAX_PAGES);
        else if (!strcmp(a, "--locality"))   o.locality   = need_int(argc, argv, &i, 0, 100);
        else if (!strcmp(a, "--cylinders"))  o.cylinders  = need_int(argc, argv, &i, 1, 1000000);
        else if (!strcmp(a, "--disk-local")) o.disk_local = need_int(argc, argv, &i, 0, 100);
        else if (!strcmp(a, "--prios"))      o.prios      = need_int(argc, argv, &i, 1, 100);
        else usage();
    }
    return o;
}

/* Generation */

/* One cpu burst of `len` ticks with one page reference per tick. A local
 * reference moves at most one page from the previous one (a loop or a
 * sequential scan); the others jump anywhere in the address space. *page
 * carries the last page from one burst to the next. */
static void emit_cpu(const options_t *o, int len, int *page)
{
    printf("    cpu %d refs=", len);
    for (int t = 0; t < len; t++) {
        if (chance(o->locality)) *page = clamp(*page + rnd(-1, 1), 0, o->pages - 1);
        else                     *page = rnd(0, o->pages - 1);
        printf(t ? ",%d" : "%d", *page);
    }
    putchar('\n');
}

/* A disk request. Localized requests stay within 5% of the disk of the
 * previous one, random ones go anywhere. *cyl carries the last cylinder. */
static void emit_disk(const options_t *o, int *cyl)
{
    int spread = o->cylinders / 20 > 0 ? o->cylinders / 20 : 1;
    if (*cyl >= 0 && chance(o->disk_local))
        *cyl = clamp(*cyl + rnd(-spread, spread), 0, o->cylinders - 1);
    else
        *cyl = rnd(0, o->cylinders - 1);
    printf("    disk %d\n", *cyl);
}

int main(int argc, char **argv)
{
    options_t o = parse(argc, argv);
    rng_seed(o.seed);

    /* The header records the exact command line, so a file can always be
     * generated again. */
    printf("# Generated by wlgen:");
    for (int i = 1; i < argc; i++) printf(" %s", argv[i]);
    printf("\n# procs=%d seed=%llu gap=%d bursts=%d cpu=%d io=%d%% disk=%d%% "
           "io_len=%d pages=%d locality=%d%% cylinders=%d disk_local=%d%% prios=%d\n\n",
           o.procs, o.seed, o.gap, o.bursts, o.cpu, o.io, o.disk, o.io_len,
           o.pages, o.locality, o.cylinders, o.disk_local, o.prios);

    int arrive = 0;
    for (int p = 1; p <= o.procs; p++) {
        printf("process g%d arrive=%d prio=%d pages=%d\n",
               p, arrive, rnd(1, o.prios), o.pages);

        int page = rnd(0, o.pages - 1), cyl = -1;
        for (int b = 0; b < o.bursts; b++) {
            emit_cpu(&o, clamp(around(o.cpu), 1, MAX_REFS), &page);
            if (b == o.bursts - 1) break;             /* end with cpu */
            if (!chance(o.io)) continue;              /* no block in this gap */
            if (chance(o.disk)) emit_disk(&o, &cyl);
            else                printf("    io %d\n", around(o.io_len));
        }
        printf("end\n\n");
        arrive += o.gap > 0 ? rnd(0, 2 * o.gap) : 0;
    }
    return 0;
}
