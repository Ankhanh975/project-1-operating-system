#include <stdarg.h>
#include "sim.h"

config_t cfg;
tick_t   now = 0;

void die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ossim: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void trace(const char *fmt, ...)
{
    if (!cfg.trace) return;
    va_list ap;
    va_start(ap, fmt);
    printf("[t=%5ld] ", now);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}
