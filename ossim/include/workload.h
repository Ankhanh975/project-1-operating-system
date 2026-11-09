/*
 * workload.h: reading a workload file.
 *
 * One directive per line. '#' starts a comment.
 *
 *   semaphore <name> <initial value>
 *   resource  <name> <instances>
 *   process <name> arrive=<tick> [prio=<n>] [pages=<n>]
 *       cpu <ticks> [refs=<p1,p2,...>]
 *       io <ticks>
 *       disk <cylinder>
 *       wait <sem>
 *       signal <sem>
 *       acquire <res> [<n>]
 *       release <res> [<n>]
 *   end
 *
 * Processes get pids 1, 2, 3, ... in the order of the file. If not given,
 * prio=5 and pages=4. A cpu op without refs= goes through pages 0, 1, ...,
 * pages-1 again and again. A refs list shorter than the burst starts again
 * from the beginning.
 */
#ifndef OSSIM_WORKLOAD_H
#define OSSIM_WORKLOAD_H

void workload_load(const char *path);

#endif
