#ifndef __STRING_BUILDER_C__
#define __STRING_BUILDER_C__

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.c"
#include "common.c"
#include "result.c"
#include "str.c"

typedef struct StringBuilder* StringBuilder;
struct StringBuilder {
    StringBuilder prev;
    size_t length;
    size_t total_length;
    char data[];
};

static Result stringAppend(StringBuilder* io_self, String string, Arena* allocator) {
    Result result = SUCCESS;
    StringBuilder self = *io_self;
    size_t previous_total_length = self == NULL ? 0 : self->total_length;
    StringBuilder next = NULL;
    CATCH(
        arenaAllocate(&next, allocator, sizeof(struct StringBuilder) + string.length),
        "Could not append string \"%s\"\n",
        string.data
    );

    next->prev = self;
    next->length = string.length;
    next->total_length = previous_total_length + next->length;
    memcpy(next->data, string.data, string.length);

    self = next;
    *io_self = self;
    return SUCCESS;
}

static Result stringAppendf(StringBuilder* io_self, Arena* allocator, char const* fmt, ...) {
    Result result;

    va_list ap;
    va_start(ap, fmt);
    size_t length = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    va_start(ap, fmt);
    StringBuilder self = *io_self;
    size_t previous_total_length = self == NULL ? 0 : self->total_length;
    StringBuilder next = NULL;
    CATCH(arenaAllocate(&next, allocator, sizeof(struct StringBuilder) + length), "Could not append format\n");

    next->prev = self;
    next->length = length;
    next->total_length = previous_total_length + next->length;
    vsnprintf(next->data, length + 1, fmt, ap);
    va_end(ap);

    self = next;
    *io_self = self;
    return SUCCESS;
}

static Result stringBuild(String* o_ret, StringBuilder const* i_self, Arena* allocator) {
    Result result = SUCCESS;
    StringBuilder self = *i_self;
    if (self == NULL) {
        *o_ret = LSTRING(NULL, 0);
        return SUCCESS;
    }
    String ret = LSTRING(NULL, self->total_length);
    CATCH(arenaAllocate(&ret.data, allocator, ret.length + 1), "Could not build string of length %zu\n", ret.length);
    ret.data[ret.length] = '\0';

    size_t offset = ret.length;
    while (self != NULL) {
        memcpy(ret.data + offset - self->length, self->data, self->length);
        offset -= self->length;
        StringBuilder prev = self->prev;
        self = prev;
    }

    *o_ret = ret;
    return SUCCESS;
}

static size_t stringBuilderLength(StringBuilder i_self) {
    return i_self == NULL ? 0 : i_self->total_length;
}

#endif
