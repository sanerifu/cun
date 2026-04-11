#ifndef __STR_C__
#define __STR_C__

#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <iso646.h>
#include <ctype.h>
#include "result.c"
#include "arena.c"

enum {
    STRING_NULL_TERMINATED = 0,
};

static char* stringSplit(size_t* o_ret_length, char** io_string, size_t* io_string_length, char const* i_delimiter, size_t i_delimiter_length) {
    if(i_delimiter_length == STRING_NULL_TERMINATED) {
        i_delimiter_length = strlen(i_delimiter);
    }
    char* string = *io_string;
    size_t string_length = *io_string_length;
    if(string_length < i_delimiter_length) {
        *o_ret_length = 0;
        return NULL;
    }
    char* ret = string;
    size_t ret_length = 0;
    bool found = false;

    while((string_length - ret_length) >= i_delimiter_length) {
        if(memcmp(string + ret_length, i_delimiter, i_delimiter_length) == 0) {
            found = true;
            break;
        }
        ret_length += 1;
    }

    if(found) {
        string_length -= ret_length + i_delimiter_length;
        string += ret_length + i_delimiter_length;
    } else {
        ret_length = string_length;
        string_length = 0;
    }

    *io_string = string;
    *io_string_length = string_length;
    *o_ret_length = ret_length;
    return ret;
}

static void stringTrim(char** io_string, size_t* io_string_length) {
    char* string = *io_string;
    size_t string_length = *io_string_length;

    while(isblank(string[0])) {
        string += 1;
        string_length -= 1;
    }
    while(isblank(string[string_length - 1])) {
        string_length -= 1;
    }

    *io_string = string;
    *io_string_length = string_length;
}

static int stringCompare(char const* i_left, size_t i_left_length, char const* i_right, size_t i_right_length) {
    i_left_length = i_left_length == STRING_NULL_TERMINATED ? strlen(i_left) : i_left_length;
    i_right_length = i_right_length == STRING_NULL_TERMINATED ? strlen(i_right) : i_right_length;

    size_t min_length = i_left_length < i_right_length ? i_left_length : i_right_length;

    return memcmp(i_left, i_right, min_length);
}

static Result stringDuplicate(char** o_string, char const* i_string, size_t i_string_length, Arena* allocator) {
    Result result = SUCCESS;
    CATCH(arenaAllocate(o_string, allocator, i_string_length + 1), "Could not duplicate string\n");
    memcpy(*o_string, i_string, i_string_length);
    (*o_string)[i_string_length] = '\0';
    return SUCCESS;
}

#endif