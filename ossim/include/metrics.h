/*
 * metrics.h: the report printed at the end of a run.
 *
 * One line per process (arrival, first run, finish, turnaround, waiting,
 * response, CPU time, page faults, disk requests), then a summary. The
 * summary lines are key=value so you can grep them in your experiment
 * scripts.
 */
#ifndef OSSIM_METRICS_H
#define OSSIM_METRICS_H

void metrics_report(void);
void metrics_write_csv(const char *path);
void metrics_print_gantt(void);

#endif
