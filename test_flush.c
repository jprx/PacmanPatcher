// A utility to test whether the patch was successful or not
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t time_load(void *p);
extern void flushd(void *p);
extern void flushi(void *p);

int main () {
    // alloc an object and ensure it's paged in + in the cache
    uint8_t *obj = malloc(0x1000);
    memset(obj, '\x41', 0x1000);

    // test dcache by measuring it
    // we assume the pmc patches are working too
    printf("cached load time: %llu\n", time_load(obj));
    flushd(obj);
    printf("flushed load time: %llu\n", time_load(obj));

    printf("if the flushed load time is not significantly larger than the cached load time, then the patch didn't work\n");

    free(obj);
    return EXIT_SUCCESS;
}
