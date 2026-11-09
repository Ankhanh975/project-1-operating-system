/*
 * mem.h: paging.
 *
 * There are cfg.frames frames, shared by all processes (global replacement).
 * On every CPU tick the running process references one page:
 *
 *   mem_access(p, page, &frame)
 *     MEM_HIT    the page is in memory and the tick runs.
 *     MEM_FAULT  a frame has been kept for (p, page), after evicting a page
 *                if no frame was free. The caller blocks p and sends a
 *                DR_PAGEIN request to the disk.
 *
 *   mem_install(frame)  the page-in is done: the page becomes valid and the
 *                       process can be made READY again.
 *
 * If every frame is still loading a page, there is nothing to evict. The
 * kernel does not call mem_access() then: the process waits until a frame
 * can be used and tries again (see kernel.c), so victim() is only called
 * when at least one frame has used && !loading.
 *
 * A replacement policy is a replacer_t. Given: FIFO.
 * TODO checkpoint 2: LRU (with timestamps) and Clock (second chance).
 */
#ifndef OSSIM_MEM_H
#define OSSIM_MEM_H

#include "process.h"

typedef enum { MEM_HIT, MEM_FAULT } mem_result_t;

typedef struct frame {
    bool   used;        /* holds a page, or a page is being loaded into it */
    bool   loading;     /* page-in not finished yet */
    int    pid;
    int    page;
    tick_t loaded_at;   /* when the page was installed (FIFO) */
    tick_t last_used;   /* last time the page was referenced (LRU) */
    bool   referenced;  /* reference bit (Clock) */
    bool   dirty;       /* not used yet (theme 5) */
} frame_t;

typedef struct replacer {
    const char *name;
    void (*init)(void);
    /* Return the index of the frame to evict. Only frames with
     * used && !loading may be chosen. */
    int  (*victim)(void);
    void (*on_hit)(int frame);       /* a page in memory was referenced */
    void (*on_install)(int frame);   /* a page has just been loaded */
} replacer_t;

extern frame_t   frames[MAX_FRAMES];
extern int       nframes;
extern replacer_t *replacer;

void         mem_init(void);
void         mem_select_replacer(const char *name);
mem_result_t mem_access(pcb_t *p, int page, int *frame_out);
void         mem_install(int frame);
void         mem_release(pcb_t *p);      /* frees all frames of p when it exits */
int          mem_free_frames(void);

/* Statistics */
extern long mem_hits, mem_faults, mem_evictions;

#endif
