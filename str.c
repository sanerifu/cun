#ifndef __STR_C__
#define __STR_C__

#include <stdlib.h>
#include <stddef.h>
#include "result.c"

typedef struct String* String;
struct String {
    size_t length;
    size_t capacity;
    char data[];
};

static Result strCreate(String* o_ret, size_t initial_capacity) {
    if(initial_capacity == 0) {
        initial_capacity = 1;
    }
    String ret = malloc(sizeof(String) + initial_capacity * sizeof(char));
    if(ret == NULL) {
        return OUT_OF_MEMORY;
    }

    ret->length = 0;
    ret->capacity = initial_capacity;
    *o_ret = ret;
    return SUCCESS;
}

static Result strAppendChar(String* io_self, char c) {
    String self = *io_self;
    if(self->length == self->capacity) {
        self = realloc(self, sizeof(String) + (self->capacity * 2) * sizeof(char));
        if(self == NULL) {
            return OUT_OF_MEMORY;
        }
        self->capacity = self->capacity * 2;
    }

    self->data[self->length] = c;
    self->length += 1;
    *io_self = self;
    return SUCCESS;
}

static Result strAppendBuffer(String* io_self, char const* buffer, size_t length) {
    for(size_t i = 0; i < length; i++) {
        Result ret = strAppendChar(io_self, buffer[i]);
        if(ret != SUCCESS) {
            return ret;
        }
    }
    return SUCCESS;
}

#endif 
