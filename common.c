#ifndef __COMMON_C__
#define __COMMON_C__

#include <stdlib.h>
#include <unistd.h>

static void freeWrapper(void* data) {
    free(*(void**)data);
}

static void closeWrapper(void* data) {
    close(*(int*)data);
}

#define CLEAN(f) __attribute__((cleanup(f##Wrapper)))

#endif
