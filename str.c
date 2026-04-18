#ifndef __STR_C__
#define __STR_C__

#include <ctype.h>
#include <iso646.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "arena.c"
#include "common.c"
#include "result.c"

typedef struct String String;
struct String {
    size_t length;
    char* data;
};

#define LSTRING(_data, _length) ((String){.length = (_length), .data = (_data)})
#define STRING_LITERAL(literal) LSTRING((literal), sizeof((literal)) - 1)
#define ZSTRING(zstr) LSTRING((zstr), strlen((zstr)))
#define CHAR_STRING(c) LSTRING(((char[]){(c)}), 1)
#define FORMAT(s) (int)((s).length), (s).data

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

    return (String){.length = end - start + 1, .data = string.data + start};
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

static Result stringFormat(String* o_ret, Arena* allocator, char const* fmt, ...) {
    Result result;
    va_list ap;
    va_start(ap, fmt);
    size_t length = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    String ret;
    CATCH(arenaAllocate(&ret.data, allocator, length + 1), "Could not allocate formatted string\n");

    va_start(ap, fmt);
    ret.length = length;
    vsnprintf(ret.data, ret.length + 1, fmt, ap);
    ret.data[ret.length] = '\0';
    va_end(ap);

    *o_ret = ret;
    return SUCCESS;
}

static Result stringFromFile(String* o_ret, Arena* allocator, FILE* fp) {
    Result result;
    String ret;

    fseek(fp, 0, SEEK_END);
    ret.length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    arenaAllocate(&ret.data, allocator, ret.length + 1);

    fread(ret.data, sizeof(char), ret.length, fp);
    ret.data[ret.length] = '\0';

    *o_ret = ret;
    return SUCCESS;
}

static uint8_t hex2nibble(char digit) {
    if ('0' <= digit and digit <= '9') {
        return digit - '0';
    } else if ('A' <= digit and digit <= 'F') {
        return digit - 'A' + 10;
    } else {
        return digit - 'a' + 10;
    }
}

static uint8_t hex2byte(char upper, char lower) {
    return (hex2nibble(upper) << 4) | (hex2nibble(lower));
}

static Result stringUrlDecode(String* o_ret, String string, Arena* allocator) {
    Result result;
    String ret;
    CATCH(arenaAllocate(&ret.data, allocator, string.length), "Could not allocate decoded string\n");
    ret.length = 0;

    for (size_t i = 0; i < string.length; i++) {
        if (string.data[i] == '%') {
            if (i > string.length - 3 || !isxdigit(string.data[i + 1]) || !isxdigit(string.data[i + 2])) {
                fprintf(stderr, "Invalid percent encoding in \"%.*s\" at index %zu\n", FORMAT(string), i);
                return INVALID_PERCENT_ENCODING;
            }
            char c = (char)hex2byte(string.data[i + 1], string.data[i + 2]);
            ret.data[ret.length] = c;
            ret.length += 1;
            i += 2;
        } else if (string.data[i] == '+') {
            ret.data[ret.length] = ' ';
            ret.length += 1;
        } else {
            ret.data[ret.length] = string.data[i];
            ret.length += 1;
        }
    }

    *o_ret = ret;
    return SUCCESS;
}

static char nibble2hex(uint8_t nibble) {
    if (0x0 <= nibble and nibble <= 0x9) {
        return nibble + '0';
    } else if (0xA <= nibble and nibble <= 0xF) {
        return nibble + 'A' - 10;
    } else {
        return '0';
    }
}

static uint16_t byte2hex(uint8_t byte) {
    uint16_t upper = (uint16_t)nibble2hex((byte & 0xF0) >> 4);
    uint16_t lower = (uint16_t)nibble2hex(byte & 0xF);
    return (upper << 8) | lower;
}

static Result stringUrlEncode(String* o_ret, String string, Arena* allocator) {
    Result result;
    String ret;

    String temp;
    Arena CLEAN(arenaDestroy) temp_allocator = {0};
    CATCH(
        arenaAllocate(&temp.data, &temp_allocator, string.length * 3),
        "Could not allocate temporary encoded string\n"
    );
    temp.length = 0;

    for (size_t i = 0; i < string.length; i++) {
        if (isalnum(string.data[i])) {
            temp.data[temp.length] = string.data[i];
            temp.length += 1;
        } else if (string.data[i] == ' ') {
            temp.data[temp.length] = '+';
            temp.length += 1;
        } else {
            uint16_t hexes = byte2hex(string.data[i]);
            char first = (hexes & 0xFF00) >> 8;
            char second = hexes & 0xFF;
            temp.data[temp.length] = '%';
            temp.data[temp.length + 1] = first;
            temp.data[temp.length + 2] = second;
            temp.length += 3;
        }
    }

    CATCH(arenaAllocate(&ret.data, allocator, temp.length + 1), "Could not allocate encoded string\n");
    ret.length = temp.length;
    memcpy(ret.data, temp.data, temp.length * sizeof(char));
    ret.data[ret.length] = '\0';

    *o_ret = ret;

    return SUCCESS;
}

#endif