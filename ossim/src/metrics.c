#include <string.h>
#include "metrics.h"
#include "process.h"
#include "kernel.h"
#include "mem.h"
#include "disk.h"
#include "sched.h"

void metrics_print_gantt(void)
{
    printf("\nCPU timeline (pid, 0 = idle, K = kernel/page-fault handling):\n  ");
    int cur = gantt_pid_at(0); tick_t start = 0;
    for (tick_t t = 1; t <= now; t++) {
        int pid = (t < now) ? gantt_pid_at(t) : -999;
        if (pid != cur) {
            if (cur == -1)     printf("[%ld-%ld] K | ", start, t - 1);
            else if (cur == 0) printf("[%ld-%ld] idle | ", start, t - 1);
            else               printf("[%ld-%ld] P%d | ", start, t - 1, cur);
            cur = pid; start = t;
        }
    }
    printf("\n");
}

void metrics_report(void)
{
    printf("\n=== ossim report: %s | sched=%s q=%d | paging=%s frames=%d replace=%s | disk=%s cyl=%d head0=%d ===\n",
           cfg.workload_path, sched->name, cfg.quantum, cfg.paging ? "on" : "off",
           cfg.frames, replacer->name, disk_sched->name, cfg.cylinders, cfg.head);
    printf("%-4s %-10s %7s %7s %7s %10s %8s %8s %6s %6s %6s\n",
           "pid", "name", "arrive", "first", "finish", "turnaround", "waiting",
           "response", "cpu", "faults", "disk");

    double sum_ta = 0, sum_wt = 0, sum_rt = 0; int done = 0;
    for (int i = 0; i < nprocs; i++) {
        pcb_t *p = &procs[i];
        long ta = p->finish >= 0 ? p->finish - p->arrival : -1;
        long rt = p->first_run >= 0 ? p->first_run - p->arrival : -1;
        printf("%-4d %-10s %7ld %7ld %7ld %10ld %8ld %8ld %6ld %6d %6d%s\n",
               p->pid, p->name, p->arrival, p->first_run, p->finish, ta, p->wait_time, rt,
               p->cpu_time, p->page_faults, p->disk_requests,
               p->state != P_TERMINATED ? "  (unfinished)" : p->finish < 0 ? "  (aborted)" : "");
        if (p->finish >= 0) { sum_ta += ta; sum_wt += p->wait_time; sum_rt += rt; done++; }
    }
    if (cfg.gantt) metrics_print_gantt();

    double util = now ? 100.0 * cpu_busy_ticks / now : 0;
    printf("\nsummary\n");
    printf("  ticks=%ld finished=%d/%d aborted=%d\n", now, done, nprocs, aborted);
    printf("  cpu_busy=%ld cpu_idle=%ld cpu_kernel=%ld utilization=%.2f%%\n",
           cpu_busy_ticks, cpu_idle_ticks, cpu_kernel_ticks, util);
    printf("  throughput=%.4f procs/tick context_switches=%ld\n",
           now ? (double)done / now : 0, ctx_switches);
    if (done) printf("  avg_turnaround=%.2f avg_waiting=%.2f avg_response=%.2f\n",
                     sum_ta / done, sum_wt / done, sum_rt / done);
    /* A reference that faults is tried again later and then hits, so
     * mem_hits is the number of references that were executed. */
    long refs = mem_hits;
    printf("  mem_refs=%ld page_faults=%ld evictions=%ld fault_rate=%.4f frame_waits=%ld\n",
           refs, mem_faults, mem_evictions, refs ? (double)mem_faults / refs : 0, frame_waits);
    printf("  disk_requests=%ld head_movement=%ld avg_disk_wait=%.2f head=%d\n",
           disk_served, disk_total_movement,
           disk_served ? (double)disk_wait_total / disk_served : 0, disk_head());
    /* From the tick of the fault until the page-in is done: waiting in the
     * disk queue, seek and transfer. */
    printf("  avg_fault_service=%.2f\n",
           faults_served ? (double)fault_service_total / faults_served : 0);
}

void metrics_write_csv(const char *path)
{
    FILE *f = strcmp(path, "-") == 0 ? stdout : fopen(path, "w");
    if (!f) die("cannot write '%s'", path);
    fprintf(f, "pid,name,arrival,first_run,finish,turnaround,waiting,response,cpu_time,"
               "block_time,page_faults,disk_requests,dispatches\n");
    for (int i = 0; i < nprocs; i++) {
        pcb_t *p = &procs[i];
        fprintf(f, "%d,%s,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%d,%d,%d\n",
                p->pid, p->name, p->arrival, p->first_run, p->finish,
                p->finish >= 0 ? p->finish - p->arrival : -1, p->wait_time,
                p->first_run >= 0 ? p->first_run - p->arrival : -1,
                p->cpu_time, p->block_time, p->page_faults, p->disk_requests, p->dispatches);
    }
    if (f != stdout) fclose(f);
}
