/*
 * disk.c: one disk, one head, one request served at a time.
 *
 * Four policies, each a pick() function over the pending requests:
 *   FCFS    (given) the oldest request.
 *   SSTF    the request nearest to the head.
 *   SCAN    keep going in the current direction, go to the edge of the
 *           disk, then turn around. The trip to the edge is passed to
 *           disk_kick() in via[] (see disk.h) so it is counted.
 *   C-LOOK  serve requests in the current direction up to the last one,
 *           then jump to the furthest request the other way and go on.
 * workloads/disk_textbook.txt with --head and --direction gives the
 * textbook numbers (tests/run.sh).
 */
#include <stdlib.h>
#include <string.h>
#include "disk.h"
#include "event.h"
#include "kernel.h"
#include "mem.h"

disk_sched_t *disk_sched = NULL;
long disk_total_movement = 0, disk_served = 0, disk_wait_total = 0;

static dreq_t pending[MAX_DISK_REQ];
static int    npending = 0;
static dreq_t active;
static bool   busy = false;
static int    head = 0;
static int    direction = +1;

/* FCFS */
static void fcfs_init(void) {}
static int  fcfs_pick(const dreq_t *p, int n, int h, int *dir, int via[2])
{
    (void)p; (void)n; (void)h; (void)dir; (void)via;
    return 0;                                   /* the oldest request */
}
static disk_sched_t ds_fcfs = { "fcfs", fcfs_init, fcfs_pick };

/* Index of the pending request nearest to cylinder h in direction dir
 * (+1: cylinder >= h, -1: cylinder <= h), or -1 if there is none. A request
 * on cylinder h itself counts for both directions. On equal distance the
 * older request (lower index) wins. Shared by SCAN and C-LOOK. */
static int nearest_in_direction(const dreq_t *p, int n, int h, int dir)
{
    int best = -1;
    for (int i = 0; i < n; i++) {
        int d = (p[i].cylinder - h) * dir;          /* distance ahead of the head */
        if (d < 0) continue;                        /* behind the head */
        if (best < 0 || d < (p[best].cylinder - h) * dir) best = i;
    }
    return best;
}

/* SSTF
 * Shortest seek time first: the request nearest to the head, whatever the
 * direction. On equal distance the older request wins. A request far from
 * the head can starve while new requests keep arriving near it
 * (Sec. 11.2.2); the textbook queue is static, so it does not happen there. */
static void sstf_init(void) {}

static int sstf_pick(const dreq_t *p, int n, int h, int *dir, int via[2])
{
    (void)dir; (void)via;
    int best = 0;
    for (int i = 1; i < n; i++)
        if (abs(p[i].cylinder - h) < abs(p[best].cylinder - h)) best = i;
    return best;
}

static disk_sched_t ds_sstf = { "sstf", sstf_init, sstf_pick };

/* SCAN (elevator)
 * Serve the nearest request in the current direction. When nothing is left
 * that way, the head still goes on to the edge of the disk (cylinder 0 or
 * cylinders-1), turns around and serves the nearest request on the way
 * back. The trip to the edge goes in via[0] so disk_kick() counts it. */
static void scan_init(void) {}

static int scan_pick(const dreq_t *p, int n, int h, int *dir, int via[2])
{
    int i = nearest_in_direction(p, n, h, *dir);
    if (i >= 0) return i;

    via[0] = *dir > 0 ? cfg.cylinders - 1 : 0;      /* to the edge first */
    *dir = -*dir;                                   /* then turn around */
    return nearest_in_direction(p, n, via[0], *dir);
}

static disk_sched_t ds_scan = { "scan", scan_init, scan_pick };

/* C-LOOK
 * Serve requests in the current direction only, and only as far as the last
 * request (no trip to the edge). Then jump straight to the request furthest
 * the other way and go on in the same direction: moving up, after the
 * highest request the head jumps to the lowest one. The jump is head
 * movement too, and it is counted like any other seek. */
static void clook_init(void) {}

static int clook_pick(const dreq_t *p, int n, int h, int *dir, int via[2])
{
    (void)via;
    int i = nearest_in_direction(p, n, h, *dir);
    if (i >= 0) return i;

    /* Nothing ahead: wrap around to the furthest request behind the head.
     * Every pending request is behind it now, so that is the lowest
     * cylinder when moving up and the highest when moving down. */
    int best = 0;
    for (int k = 1; k < n; k++)
        if ((p[k].cylinder - p[best].cylinder) * *dir < 0) best = k;
    return best;
}

static disk_sched_t ds_clook = { "clook", clook_init, clook_pick };

static disk_sched_t *all[] = { &ds_fcfs, &ds_sstf, &ds_scan, &ds_clook };

void disk_select_sched(const char *name)
{
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        if (strcmp(all[i]->name, name) == 0) { disk_sched = all[i]; disk_sched->init(); return; }
    die("unknown disk scheduler '%s' (fcfs, sstf, scan, clook)", name);
}

void disk_init(void)
{
    if (cfg.cylinders <= 0) die("--cylinders must be positive");
    if (cfg.head < 0 || cfg.head >= cfg.cylinders)
        die("--head must be in 0..%d", cfg.cylinders - 1);
    if (cfg.seek_speed <= 0) die("--seek-speed must be positive");
    if (cfg.seek_cost < 0 || cfg.transfer_cost < 0) die("disk costs must be >= 0");
    npending = 0; busy = false; head = cfg.head; direction = cfg.direction;
}

int  disk_head(void)    { return head; }
int  disk_pending(void) { return npending; }
bool disk_busy(void)    { return busy; }

/* If the disk is free, take the next request, move the head and add the
 * event for when it will be done. */
void disk_kick(void)
{
    if (busy || npending == 0) return;
    int via[2] = { -1, -1 };
    int i = disk_sched->pick(pending, npending, head, &direction, via);
    if (i < 0 || i >= npending) die("disk scheduler returned bad index %d", i);
    active = pending[i];
    for (int j = i; j < npending - 1; j++) pending[j] = pending[j + 1];
    npending--;

    long move = 0;
    int  at = head;
    for (int k = 0; k < 2; k++) {
        if (via[k] < 0) continue;
        if (via[k] >= cfg.cylinders) die("disk scheduler: via cylinder %d out of range", via[k]);
        move += labs((long)via[k] - at);
        at = via[k];
    }
    move += labs((long)active.cylinder - at);
    disk_total_movement += move;
    disk_wait_total     += now - active.submitted;
    head = active.cylinder;
    busy = true;
    tick_t service = (move * cfg.seek_cost + cfg.seek_speed - 1) / cfg.seek_speed
                     + cfg.transfer_cost;
    if (service < 1) service = 1;
    trace("DISK start %s P%d cyl %d (move %ld, done at t=%ld)",
          active.type == DR_PAGEIN ? "page-in" : "file-io",
          active.pid, active.cylinder, move, now + service);
    event_push(now + service, EV_DISK_DONE, active.pid);
}

void disk_submit(dreq_t r)
{
    if (npending >= MAX_DISK_REQ) die("disk queue overflow");
    if (r.cylinder < 0 || r.cylinder >= cfg.cylinders)
        die("P%d: cylinder %d out of range 0..%d", r.pid, r.cylinder, cfg.cylinders - 1);
    r.submitted = now;
    pending[npending++] = r;
    pcb_t *p = proc_by_pid(r.pid);
    if (p) p->disk_requests++;
    trace("DISK submit %s P%d cyl %d (queue %d)",
          r.type == DR_PAGEIN ? "page-in" : "file-io", r.pid, r.cylinder, npending);
}

void disk_on_complete(void)
{
    if (!busy) die("EV_DISK_DONE while the disk is idle");
    busy = false;
    disk_served++;
    pcb_t *p = proc_by_pid(active.pid);
    if (!p) die("disk completion for unknown pid %d", active.pid);

    if (active.type == DR_PAGEIN) {
        mem_install(active.frame);
        kernel_make_ready(p);              /* it will retry the same reference */
    } else {
        kernel_advance(p);                 /* the `disk C` op is done */
    }
    /* The kernel starts the next request with disk_kick() after all events
     * of this tick, so requests that arrive now also have a chance. */
}
