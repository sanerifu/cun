#ifndef __STRING_BUILDER_C__
#define __STRING_BUILDER_C__

#include <stdlib.h>
#include <stddef.h>
#include "result.c"
#include "arena.c"
#include <string.h>

typedef struct StringBuilder* StringBuilder;
struct StringBuilder {
    StringBuilder prev;
    size_t length;
    size_t total_length;
    char data[];
};

static Result stringAppendChar(StringBuilder* io_self, char c, Arena* allocator) {
    Result result = SUCCESS;
    StringBuilder self = *io_self;
    size_t previous_total_length = self == NULL ? 0 : self->total_length;
    StringBuilder next = NULL;
    CATCH(arenaAllocate(&next, allocator, sizeof(struct StringBuilder) + 1), "Could not append char \'%c\'\n", c);

    next->prev = self;
    next->length = 1;
    next->total_length = previous_total_length + next->length;
    next->data[0] = c;

    self = next;
    *io_self = self;
    return SUCCESS;
}

static Result stringAppendBuffer(StringBuilder* io_self, char const* string, size_t length, Arena* allocator) {
    Result result = SUCCESS;
    StringBuilder self = *io_self;
    size_t previous_total_length = self == NULL ? 0 : self->total_length;
    StringBuilder next = NULL;
    CATCH(arenaAllocate(&next, allocator, sizeof(struct StringBuilder) + length), "Could not append string \"%s\"\n", string);

    next->prev = self;
    next->length = length;
    next->total_length = previous_total_length + next->length;
    memcpy(next->data, string, length);

    self = next;
    *io_self = self;
    return SUCCESS;
}

static Result stringAppendString(StringBuilder* io_self, char const* string, Arena* allocator) {
    return stringAppendBuffer(io_self, string, strlen(string), allocator);
}

static Result stringBuild(char** o_data, size_t* o_length, StringBuilder const* i_self, Arena* allocator) {
    Result result = SUCCESS;
    StringBuilder self = *i_self;
    size_t length = self->total_length;
    char* data = NULL;
    CATCH(arenaAllocate(&data, allocator, length + 1), "Could not build string of length %zu\n", length);
    data[length] = '\0';

    size_t offset = length;
    while(self != NULL) {
        memcpy(data + offset - self->length, self->data, self->length);
        offset -= self->length;
        StringBuilder prev = self->prev;
        self = prev;
    }

    *o_data = data;
    *o_length = length;
    return SUCCESS;
}

#endif 
