#ifndef __STR_C__
#define __STR_C__

#include <ctype.h>
#include <iso646.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "arena.c"
#include "result.c"

typedef struct String String;
struct String {
    size_t length;
    char* data;
};

#define STRING_LITERAL(literal) ((String){.length = sizeof(literal) - 1, .data = (char*)(literal)})
#define ZSTRING(zstr) ((String){.length = strlen((zstr)), .data = (zstr)})

enum {
    STRING_NULL_TERMINATED = 0,
};

static String stringSplit(String* io_string, String delimiter) {
    String string = *io_string;
    if (string.length < delimiter.length) {
        return (String){.length = 0, .data = NULL};
    }
    String ret = (String){.length = 0, .data = string.data};
    bool found = false;

    while ((string.length - ret.length) >= delimiter.length) {
        if (memcmp(string.data + ret.length, delimiter.data, delimiter.length) == 0) {
            found = true;
            break;
        }
        ret.length += 1;
    }

    if (found) {
        string.length -= ret.length + delimiter.length;
        string.data += ret.length + delimiter.length;
    } else {
        ret.length = string.length;
        string.length = 0;
    }

    *io_string = string;
    return ret;
}

static String stringTrim(String string) {
    if (string.length == 0) {
        return string;
    }
    size_t start = 0;
    size_t end = string.length - 1;
    while (start < string.length and isblank(string.data[start])) {
        start += 1;
    }
    while (end > 0 and isblank(string.data[end])) {
        end -= 1;
    }

    return (String){.length = end - start, .data = string.data + start};
}

static int stringCompare(String left, String right) {
    size_t min_length = left.length < right.length ? left.length : right.length;
    int result = memcmp(left.data, right.data, min_length);
    if (result == 0) {
        return (int)left.length - (int)right.length;
    } else {
        return result;
    }
}

static Result stringDuplicate(String* o_ret, String string, Arena* allocator) {
    Result result = SUCCESS;
    String ret;
    CATCH(arenaAllocate(&ret.data, allocator, string.length + 1), "Could not duplicate string\n");
    memcpy(ret.data, string.data, string.length);
    ret.length = string.length;
    ret.data[ret.length] = '\0';
    *o_ret = ret;
    return SUCCESS;
}

#endif