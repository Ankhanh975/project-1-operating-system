/*
 * workload.c: reads the workload file. The format is in include/workload.h.
 */
#include <ctype.h>
#include <string.h>
#include "workload.h"
#include "process.h"
#include "sync.h"
#include "sim.h"

static int lineno = 0;

static void perr(const char *msg, const char *tok)
{
    die("workload line %d: %s%s%s", lineno, msg, tok ? ": " : "", tok ? tok : "");
}

/* The whole token must be a number, "5x" is an error. */
static int num(const char *tok)
{
    char *end;
    long v = strtol(tok, &end, 10);
    if (end == tok || *end || v < -1000000000L || v > 1000000000L) perr("not an integer", tok);
    return (int)v;
}

/* For a token "key=value", return the value. NULL if it is a different key. */
static const char *kv(const char *tok, const char *key)
{
    size_t k = strlen(key);
    if (strncmp(tok, key, k) == 0 && tok[k] == '=') return tok + k + 1;
    return NULL;
}

static void parse_refs(op_t *op, const char *list)
{
    char buf[4096];
    strncpy(buf, list, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    op->nrefs = 0;
    for (char *t = strtok(buf, ","); t; t = strtok(NULL, ",")) {
        if (op->nrefs >= MAX_REFS) perr("too many page references", NULL);
        op->refs[op->nrefs++] = num(t);
    }
    if (op->nrefs == 0) perr("empty refs= list", NULL);
}

void workload_load(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) die("cannot open workload '%s'", path);

    char   line[8192];
    pcb_t *cur = NULL;
    nprocs = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *hash = strchr(line, '#');
        if (hash) *hash = 0;

        char *tok[16];
        int   nt = 0;
        for (char *t = strtok(line, " \t\r\n"); t && nt < 16; t = strtok(NULL, " \t\r\n"))
            tok[nt++] = t;
        if (nt == 0) continue;

        if (strcmp(tok[0], "semaphore") == 0) {
            if (cur) perr("'semaphore' inside a process block", NULL);
            if (nt != 3) perr("usage: semaphore <name> <initial>", NULL);
            if (num(tok[2]) < 0) perr("semaphore value must be >= 0", tok[2]);
            sem_create(tok[1], num(tok[2]));
        } else if (strcmp(tok[0], "resource") == 0) {
            if (cur) perr("'resource' inside a process block", NULL);
            if (nt != 3) perr("usage: resource <name> <instances>", NULL);
            if (num(tok[2]) <= 0) perr("a resource needs at least 1 instance", tok[2]);
            res_create(tok[1], num(tok[2]));
        } else if (strcmp(tok[0], "process") == 0) {
            if (cur) perr("missing 'end' before new process", NULL);
            if (nprocs >= MAX_PROCS) perr("too many processes", NULL);
            if (nt < 2) perr("usage: process <name> arrive=<t> [prio=<n>] [pages=<n>]", NULL);
            cur = &procs[nprocs++];
            memset(cur, 0, sizeof(*cur));
            cur->pid = nprocs;
            strncpy(cur->name, tok[1], MAX_NAME - 1);
            cur->priority = 5;
            cur->pages = 4;
            cur->arrival = 0;
            for (int i = 2; i < nt; i++) {
                const char *v;
                if      ((v = kv(tok[i], "arrive"))) cur->arrival  = num(v);
                else if ((v = kv(tok[i], "prio")))   cur->priority = num(v);
                else if ((v = kv(tok[i], "pages")))  cur->pages    = num(v);
                else perr("unknown process attribute", tok[i]);
            }
            if (cur->pages <= 0 || cur->pages > MAX_PAGES) perr("pages out of range", NULL);
            if (cur->arrival < 0) perr("arrive must be >= 0", NULL);
        } else if (strcmp(tok[0], "end") == 0) {
            if (!cur) perr("'end' without process", NULL);
            if (cur->nops == 0) perr("process has no operations", cur->name);
            cur = NULL;
        } else {
            if (!cur) perr("operation outside a process block", tok[0]);
            if (cur->nops >= MAX_OPS) perr("too many operations", cur->name);
            op_t *op = &cur->ops[cur->nops];
            memset(op, 0, sizeof(*op));
            if (strcmp(tok[0], "cpu") == 0) {
                if (nt < 2) perr("usage: cpu <ticks> [refs=a,b,c]", NULL);
                op->type = OP_CPU; op->length = num(tok[1]);
                if (op->length <= 0) perr("cpu ticks must be positive", NULL);
                for (int i = 2; i < nt; i++) {
                    const char *v = kv(tok[i], "refs");
                    if (!v) perr("unknown cpu attribute", tok[i]);
                    parse_refs(op, v);
                }
                for (int i = 0; i < op->nrefs; i++)
                    if (op->refs[i] < 0 || op->refs[i] >= cur->pages)
                        perr("page reference outside the process's pages", cur->name);
                cur->total_cpu += op->length;
            } else if (strcmp(tok[0], "io") == 0) {
                if (nt != 2) perr("usage: io <ticks>", NULL);
                op->type = OP_IO; op->length = num(tok[1]);
                if (op->length <= 0) perr("io ticks must be positive", NULL);
            } else if (strcmp(tok[0], "disk") == 0) {
                if (nt != 2) perr("usage: disk <cylinder>", NULL);
                op->type = OP_DISK; op->cylinder = num(tok[1]);
                if (op->cylinder < 0 || op->cylinder >= cfg.cylinders)
                    perr("disk cylinder outside 0..cylinders-1 (see --cylinders)", tok[1]);
            } else if (strcmp(tok[0], "wait") == 0 || strcmp(tok[0], "signal") == 0) {
                if (nt != 2) perr("usage: wait|signal <semaphore>", NULL);
                op->type = tok[0][0] == 'w' ? OP_WAIT : OP_SIGNAL;
                strncpy(op->name, tok[1], MAX_NAME - 1);
            } else if (strcmp(tok[0], "acquire") == 0 || strcmp(tok[0], "release") == 0) {
                if (nt < 2 || nt > 3) perr("usage: acquire|release <resource> [n]", NULL);
                op->type = tok[0][0] == 'a' ? OP_ACQUIRE : OP_RELEASE;
                strncpy(op->name, tok[1], MAX_NAME - 1);
                op->length = nt == 3 ? num(tok[2]) : 1;
                if (op->length <= 0) perr("resource count must be positive", NULL);
            } else {
                perr("unknown directive", tok[0]);
            }
            cur->nops++;
        }
    }
    if (cur) perr("missing 'end' at end of file", cur->name);
    fclose(f);
    if (nprocs == 0) die("workload '%s' defines no processes", path);

    /* Checks that need the whole file. */
    for (int i = 0; i < nprocs; i++)
        for (int k = 0; k < procs[i].nops; k++) {
            op_t *op = &procs[i].ops[k];
            if ((op->type == OP_WAIT || op->type == OP_SIGNAL) && !sem_find(op->name))
                die("process %s uses undefined semaphore '%s'", procs[i].name, op->name);
            if ((op->type == OP_ACQUIRE || op->type == OP_RELEASE) && !res_find(op->name))
                die("process %s uses undefined resource '%s'", procs[i].name, op->name);
        }
}
