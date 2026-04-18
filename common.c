#ifndef __COMMON_C__
#define __COMMON_C__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void freeWrapper(void* data) {
    free(*(void**)data);
}

static void closeWrapper(void* data) {
    close(*(int*)data);
}

static void fcloseWrapper(void* data) {
    FILE* fp = *(FILE**)data;
    if (fp != NULL) {
        fclose(fp);
    }
}

#define CLEAN(f) __attribute__((cleanup(f##Wrapper)))

#endif
