#ifndef __ARENA_C__
#define __ARENA_C__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "result.c"

typedef struct Arena* Arena;
struct Arena {
    Arena prev;
    size_t size;
    size_t capacity;
    char data[];
};

static Result arenaAllocate(void* o_ret, Arena* io_self, size_t size) {
    Arena self = *io_self;
    if (self == NULL) {
        self = malloc(sizeof(struct Arena) + size);
        if (self == NULL) {
            THROW(OUT_OF_MEMORY, "");
        }
        self->capacity = size;
        self->size = 0;
        self->prev = NULL;
    } else if (self->capacity - self->size < size) {
        size_t new_size = size > self->capacity * 2 ? size : self->capacity * 2;
        Arena new_node = malloc(sizeof(struct Arena) + new_size);
        if (new_node == NULL) {
            THROW(OUT_OF_MEMORY, "");
        }
        new_node->capacity = new_size;
        new_node->size = 0;
        new_node->prev = self;
        self = new_node;
    }

    *(void**)o_ret = self->data + self->size;
    self->size += size;
    *io_self = self;
    return SUCCESS;
}

static void arenaDestroy(Arena* io_self) {
    Arena self = *io_self;
    while (self != NULL) {
        Arena prev = self->prev;
        free(self);
        self = prev;
    }
    *io_self = NULL;
}

static void arenaDestroyWrapper(void* data) {
    arenaDestroy((Arena*)data);
}

#endif