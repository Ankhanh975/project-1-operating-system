/*
 * main.c: reads the command line, sets everything up, runs, prints the report.
 *
 *   ./ossim workloads/basic.txt --sched rr --quantum 4 --gantt
 *   ./ossim workloads/paging.txt --frames 3 --replace fifo --trace
 *   ./ossim --help
 */
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "sim.h"
#include "event.h"
#include "kernel.h"
#include "sched.h"
#include "mem.h"
#include "disk.h"
#include "workload.h"
#include "metrics.h"

static void usage(void)
{
    printf("usage: ossim <workload> [options]\n"
           "  --sched NAME        fcfs | rr | sjf | srtf | prio | mlfq      (default fcfs)\n"
           "  --quantum N         RR / MLFQ time quantum in ticks          (default 4)\n"
           "  --frames N          physical frames                          (default 64)\n"
           "  --replace NAME      fifo | lru | clock                        (default fifo)\n"
           "  --disk NAME         fcfs | sstf | scan | clook                (default fcfs)\n"
           "  --cylinders N       disk cylinders 0..N-1                     (default 200)\n"
           "  --head N            initial head cylinder                     (default 0)\n"
           "  --direction up|down initial head direction (SCAN, C-LOOK)     (default up)\n"
           "  --seek-speed N      cylinders crossed per tick of seeking     (default 10)\n"
           "  --seek-cost N       seek ticks = ceil(moved*N/seek-speed)     (default 1)\n"
           "  --transfer-cost N   ticks per request once positioned         (default 1)\n"
           "  --max-ticks N       stop after N ticks                        (default %d)\n"
           "  --deadlock-every N  run deadlock_check() every N ticks        (default 0=off)\n"
           "  --no-paging         disable demand paging (no page faults)\n"
           "  --trace             print every state change\n"
           "  --gantt             print the compressed CPU timeline\n"
           "  --csv PATH          per-process metrics as CSV ('-' = stdout)\n",
           MAX_TICKS_DEF);
}

static int need_int(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) die("option %s needs a value", argv[*i]);
    const char *opt = argv[*i], *v = argv[++*i];
    char *end;
    errno = 0;
    long n = strtol(v, &end, 10);
    if (errno || end == v || *end || n < INT_MIN || n > INT_MAX)
        die("option %s needs an integer, got '%s'", opt, v);
    return (int)n;
}

static const char *need_str(int argc, char **argv, int *i)
{
    if (*i + 1 >= argc) die("option %s needs a value", argv[*i]);
    return argv[++*i];
}

int main(int argc, char **argv)
{
    cfg = (config_t){
        .sched_name = "fcfs", .quantum = 4, .frames = 64, .replace_name = "fifo",
        .disk_name = "fcfs", .cylinders = 200, .head = 0, .direction = +1,
        .seek_cost = 1, .seek_speed = 10, .transfer_cost = 1,
        .max_ticks = MAX_TICKS_DEF, .deadlock_interval = 0,
        .paging = true, .trace = false, .gantt = false, .csv_path = NULL,
    };

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if      (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
        else if (!strcmp(a, "--sched"))          cfg.sched_name = need_str(argc, argv, &i);
        else if (!strcmp(a, "--quantum"))        cfg.quantum = need_int(argc, argv, &i);
        else if (!strcmp(a, "--frames"))         cfg.frames = need_int(argc, argv, &i);
        else if (!strcmp(a, "--replace"))        cfg.replace_name = need_str(argc, argv, &i);
        else if (!strcmp(a, "--disk"))           cfg.disk_name = need_str(argc, argv, &i);
        else if (!strcmp(a, "--cylinders"))      cfg.cylinders = need_int(argc, argv, &i);
        else if (!strcmp(a, "--head"))           cfg.head = need_int(argc, argv, &i);
        else if (!strcmp(a, "--direction")) {
            const char *d = need_str(argc, argv, &i);
            if      (!strcmp(d, "up"))   cfg.direction = +1;
            else if (!strcmp(d, "down")) cfg.direction = -1;
            else die("--direction must be 'up' or 'down'");
        }
        else if (!strcmp(a, "--seek-speed"))     cfg.seek_speed = need_int(argc, argv, &i);
        else if (!strcmp(a, "--seek-cost"))      cfg.seek_cost = need_int(argc, argv, &i);
        else if (!strcmp(a, "--transfer-cost"))  cfg.transfer_cost = need_int(argc, argv, &i);
        else if (!strcmp(a, "--max-ticks"))      cfg.max_ticks = need_int(argc, argv, &i);
        else if (!strcmp(a, "--deadlock-every")) cfg.deadlock_interval = need_int(argc, argv, &i);
        else if (!strcmp(a, "--no-paging"))      cfg.paging = false;
        else if (!strcmp(a, "--trace"))          cfg.trace = true;
        else if (!strcmp(a, "--gantt"))          cfg.gantt = true;
        else if (!strcmp(a, "--csv"))            cfg.csv_path = need_str(argc, argv, &i);
        else if (a[0] == '-')                    die("unknown option '%s' (try --help)", a);
        else if (!cfg.workload_path)             cfg.workload_path = a;
        else                                     die("unexpected argument '%s'", a);
    }
    if (!cfg.workload_path) { usage(); return 1; }
    if (cfg.quantum <= 0) die("--quantum must be positive");
    if (cfg.max_ticks <= 0) die("--max-ticks must be positive");

    workload_load(cfg.workload_path);
    event_init();
    sched_select(cfg.sched_name);
    mem_select_replacer(cfg.replace_name);
    disk_select_sched(cfg.disk_name);
    mem_init();
    disk_init();
    kernel_init();

    kernel_run();

    metrics_report();
    if (cfg.csv_path) metrics_write_csv(cfg.csv_path);
    return 0;
}
