/*
 * mem.c: paging with global replacement.
 *
 * FIFO is given: it evicts the page that was loaded first.
 * TODO checkpoint 2: LRU with frames[i].last_used, and Clock with a hand
 * that goes round the frame table and frames[i].referenced (set on every
 * hit, cleared when the hand passes). Both are replacer_t, like FIFO. Put
 * them in all[] below when they work.
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
 * TODO: the victim is the frame with the smallest last_used, among frames
 * with used && !loading. last_used has to be updated on every hit. Look at
 * mem_access() below before you do it again in on_hit. */
static void lru_init(void) { die("replacement 'lru' is not implemented yet (src/mem.c)"); }
static replacer_t rep_lru = { "lru", lru_init, NULL, NULL, NULL };

/* Clock
 * TODO: second chance. Keep a `hand` that stays between calls. Move it
 * round the frames: if referenced is true, clear it and go on; the first
 * frame with referenced == false is the victim. Skip frames that are free
 * or still loading. */
static void clock_init(void) { die("replacement 'clock' is not implemented yet (src/mem.c)"); }
static replacer_t rep_clock = { "clock", clock_init, NULL, NULL, NULL };

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
