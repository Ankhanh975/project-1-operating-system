/*
 * mem.c: paging with global replacement.
 *
 * Three replacement policies, each a replacer_t (see mem.h):
 *   FIFO   (given) evicts the page that was loaded first.
 *   LRU    (checkpoint 2) evicts the page with the oldest last_used.
 *   Clock  (checkpoint 2) second chance with a hand over the frame table.
 * mem_access() and mem_install() keep last_used and referenced up to date
 * for every policy, so a policy only has to choose the victim.
 */
#include <string.h>
#include "mem.h"

frame_t     frames[MAX_FRAMES];
int         nframes = 0;
replacer_t *replacer = NULL;
long        mem_hits = 0, mem_faults = 0, mem_evictions = 0;

/* FIFO */
static void fifo_init(void) {}

static int fifo_victim(void)
{
    int best = -1;
    for (int i = 0; i < nframes; i++) {
        if (!frames[i].used || frames[i].loading) continue;
        if (best < 0 || frames[i].loaded_at < frames[best].loaded_at) best = i;
    }
    return best;
}

static void fifo_on_hit(int f)     { (void)f; }
static void fifo_on_install(int f) { frames[f].loaded_at = now; }

static replacer_t rep_fifo = { "fifo", fifo_init, fifo_victim, fifo_on_hit, fifo_on_install };

/* LRU
 * The victim is the page that has gone longest without a reference, that is
 * the evictable frame with the smallest last_used. mem_access() stamps
 * last_used on every hit and mem_install() stamps it when the page arrives,
 * so the hooks have nothing left to do. On equal timestamps the lower frame
 * index wins, which keeps runs deterministic. */
static void lru_init(void) {}

static int lru_victim(void)
{
    int best = -1;
    for (int i = 0; i < nframes; i++) {
        if (!frames[i].used || frames[i].loading) continue;
        if (best < 0 || frames[i].last_used < frames[best].last_used) best = i;
    }
    return best;
}

static void lru_on_hit(int f)     { (void)f; }   /* last_used set by mem_access() */
static void lru_on_install(int f) { (void)f; }   /* last_used set by mem_install() */

static replacer_t rep_lru = { "lru", lru_init, lru_victim, lru_on_hit, lru_on_install };

/* Clock (second chance)
 * The frames form a circle and `hand` points at the next candidate. It stays
 * where it is between faults. At a fault the hand moves round:
 *   - free or still loading frame: skip it, it cannot be evicted;
 *   - referenced == true: clear the bit (the second chance) and go on;
 *   - referenced == false: this is the victim.
 * The hand stops one frame past the victim, so the new page is the last one
 * the hand will look at again. The reference bit is set by mem_access() on a
 * hit and by mem_install() when the page arrives.
 * mem.h guarantees at least one evictable frame, so after at most one full
 * turn every evictable bit is clear and the second turn finds a victim. */
static int clock_hand = 0;

static void clock_init(void) { clock_hand = 0; }

static int clock_victim(void)
{
    for (int step = 0; step < 2 * nframes; step++) {
        int f = clock_hand;
        clock_hand = (clock_hand + 1) % nframes;
        if (!frames[f].used || frames[f].loading) continue;
        if (frames[f].referenced) { frames[f].referenced = false; continue; }
        return f;
    }
    return -1;   /* no evictable frame; mem_access() reports it */
}

static void clock_on_hit(int f)     { (void)f; }   /* bit set by mem_access() */
static void clock_on_install(int f) { (void)f; }   /* bit set by mem_install() */

static replacer_t rep_clock = { "clock", clock_init, clock_victim, clock_on_hit, clock_on_install };

static replacer_t *all[] = { &rep_fifo, &rep_lru, &rep_clock };

void mem_select_replacer(const char *name)
{
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++)
        if (strcmp(all[i]->name, name) == 0) { replacer = all[i]; replacer->init(); return; }
    die("unknown replacement policy '%s' (fifo, lru, clock)", name);
}

void mem_init(void)
{
    nframes = cfg.frames;
    if (nframes <= 0 || nframes > MAX_FRAMES) die("--frames must be in 1..%d", MAX_FRAMES);
    memset(frames, 0, sizeof(frames));
    for (int i = 0; i < nprocs; i++)
        for (int pg = 0; pg < MAX_PAGES; pg++)
            procs[i].page_table[pg] = (pte_t){ .frame = -1, .valid = false };
}

int mem_free_frames(void)
{
    int c = 0;
    for (int i = 0; i < nframes; i++) if (!frames[i].used) c++;
    return c;
}

static int find_free_frame(void)
{
    for (int i = 0; i < nframes; i++) if (!frames[i].used) return i;
    return -1;
}

mem_result_t mem_access(pcb_t *p, int page, int *frame_out)
{
    if (page < 0 || page >= p->pages)
        die("P%d referenced page %d outside its address space (pages=%d)", p->pid, page, p->pages);

    pte_t *pte = &p->page_table[page];
    if (pte->valid) {
        mem_hits++;
        frames[pte->frame].last_used  = now;
        frames[pte->frame].referenced = true;
        replacer->on_hit(pte->frame);
        *frame_out = pte->frame;
        return MEM_HIT;
    }

    /* Page fault. Take a free frame, or evict a page if there is none. */
    mem_faults++;
    p->page_faults++;
    int f = find_free_frame();
    if (f < 0) {
        f = replacer->victim();
        if (f < 0) die("no evictable frame: every frame is loading (frames=%d too small?)", nframes);
        pcb_t *owner = proc_by_pid(frames[f].pid);
        if (owner) owner->page_table[frames[f].page] = (pte_t){ .frame = -1, .valid = false };
        mem_evictions++;
        trace("P%d page %d FAULT: evict P%d page %d from frame %d",
              p->pid, page, frames[f].pid, frames[f].page, f);
    } else {
        trace("P%d page %d FAULT: free frame %d", p->pid, page, f);
    }
    frames[f] = (frame_t){ .used = true, .loading = true, .pid = p->pid, .page = page,
                           .loaded_at = now, .last_used = now, .referenced = false };
    *frame_out = f;
    return MEM_FAULT;
}

void mem_install(int f)
{
    frame_t *fr = &frames[f];
    if (!fr->used || !fr->loading) die("mem_install on frame %d that is not loading", f);
    pcb_t *p = proc_by_pid(fr->pid);
    if (!p) die("mem_install: unknown pid %d", fr->pid);
    fr->loading   = false;
    fr->loaded_at = now;
    fr->last_used = now;
    fr->referenced = true;
    p->page_table[fr->page] = (pte_t){ .frame = f, .valid = true };
    replacer->on_install(f);
    trace("P%d page %d installed in frame %d", p->pid, fr->page, f);
}

void mem_release(pcb_t *p)
{
    for (int i = 0; i < nframes; i++)
        if (frames[i].used && frames[i].pid == p->pid)
            frames[i] = (frame_t){ 0 };
    for (int pg = 0; pg < p->pages; pg++)
        p->page_table[pg] = (pte_t){ .frame = -1, .valid = false };
}
