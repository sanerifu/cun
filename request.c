#ifndef __REQUEST_C__
#define __REQUEST_C__

#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>

#include "arena.c"
#include "common.c"
#include "result.c"
#include "str.c"
#include "string_builder.c"

typedef enum {
    GET_REQUEST,
    POST_REQUEST,
    HEAD_REQUEST,
    PUT_REQUEST,
    PATCH_REQUEST,
    DELETE_REQUEST
} RequestMethod;

typedef enum {
    HTTP_1_0 = 0x0100,
    HTTP_1_1 = 0x0101
} HttpVersion;

typedef struct RequestHeader RequestHeader;
struct RequestHeader {
    RequestMethod method;
    String path;
    HttpVersion version;

    String queries;
    String user_agent;
    size_t content_length;
};

static Result readRequestHeader(
    String* o_header,
    StringBuilder* o_body_builder,
    int socket,
    Arena* allocator,
    Arena* temp_allocator
) {
    Result result = SUCCESS;
    StringBuilder header_builder = {0};
    size_t initial_body_length = 0;
    {
        char buf[BUFSIZ];
        String buffer = LSTRING(buf, sizeof(buf));
        int read_size = 0;
        while ((read_size = recv(socket, buffer.data, buffer.length - 1, 0)) != 0) {
            WRAP(RECEIVE_ERROR, read_size, "Could not receive: %s\n", strerror(errno));
            char* end_position = strstr(buf, "\r\n\r\n");
            if (end_position != NULL) {
                size_t last_header_length = end_position - buf;
                CATCH(
                    stringAppend(&header_builder, LSTRING(buffer.data, last_header_length), temp_allocator),
                    "Could not push last part of header\n"
                );
                CATCH(
                    stringAppend(
                        o_body_builder,
                        LSTRING(end_position + 4, read_size - last_header_length - 4),
                        temp_allocator
                    ),
                    "Could not push first part of body\n"
                );
                break;
            }
            CATCH(
                stringAppend(&header_builder, LSTRING(buffer.data, read_size), temp_allocator),
                "Could not append the read buffer\n"
            );
        }
    }

    CATCH(stringBuild(o_header, &header_builder, allocator), "Could not build request header string\n");

    return SUCCESS;
}

static Result parseRequestHeader(RequestHeader* o_ret, String header, Arena* allocator) {
    Result result = SUCCESS;
    String line = {0};
    RequestHeader ret = {0};
    Arena CLEAN(arenaDestroy) temp_allocator = {0};

    String http_start = stringSplit(&header, STRING_LITERAL("\r\n"));
    String method = stringSplit(&http_start, STRING_LITERAL(" "));
    if (stringCompare(method, STRING_LITERAL("GET")) == 0) {
        ret.method = GET_REQUEST;
    } else if (stringCompare(method, STRING_LITERAL("POST")) == 0) {
        ret.method = POST_REQUEST;
    } else if (stringCompare(method, STRING_LITERAL("HEAD")) == 0) {
        ret.method = HEAD_REQUEST;
    } else if (stringCompare(method, STRING_LITERAL("PUT")) == 0) {
        ret.method = PUT_REQUEST;
    } else if (stringCompare(method, STRING_LITERAL("PATCH")) == 0) {
        ret.method = PATCH_REQUEST;
    } else if (stringCompare(method, STRING_LITERAL("DELETE")) == 0) {
        ret.method = DELETE_REQUEST;
    } else {
        fprintf(stderr, "Unrecognized HTTP method: \"%.*s\"\n", FORMAT(method));
        return UNRECOGNIZED_HTTP_METHOD;
    }
    ret.queries = stringSplit(&http_start, STRING_LITERAL(" "));
    String path = stringSplit(&ret.queries, STRING_LITERAL("?"));
    CATCH(stringUrlDecode(&ret.path, path, allocator), "Could not decode URL string");

    String http_version = stringSplit(&http_start, STRING_LITERAL(" "));
    if(stringCompare(http_version, STRING_LITERAL("HTTP/1.1")) == 0) {
        ret.version = HTTP_1_1;
    } else if(stringCompare(http_version, STRING_LITERAL("HTTP/1.0")) == 0) {
        ret.version = HTTP_1_0;
    } else {
        fprintf(stderr, "Unsupported HTTP version \"%.*s\"", FORMAT(http_version));
        return UNSUPPORTED_HTTP_VERSION;
    }

    while ((line = stringSplit(&header, STRING_LITERAL("\r\n"))).data) {
        String key = stringSplit(&line, STRING_LITERAL(":"));
        String value = line;

        if (stringCompare(key, STRING_LITERAL("Content-Length")) == 0) {
            String value_copy = {0};
            CATCH(
                stringDuplicate(&value_copy, value, &temp_allocator),
                "Could not allocate null-terminated string for content length"
            );
            ret.content_length = strtoull(value_copy.data, NULL, 10);
        } else if (stringCompare(key, STRING_LITERAL("User-Agent")) == 0) {
            ret.user_agent = stringTrim(value);
        }
    }

    *o_ret = ret;
}

static Result readRequestBody(
    String* o_body,
    StringBuilder* io_body_builder,
    int socket,
    RequestHeader const* header,
    Arena* allocator,
    Arena* temp_allocator
) {
    Result result;

    size_t remaining_body_length = header->content_length - stringBuilderLength(*io_body_builder);
    if (header->content_length > stringBuilderLength(*io_body_builder)) {
        char* remaining_body = NULL;
        CATCH(
            arenaAllocate(&remaining_body, temp_allocator, remaining_body_length),
            "Could not allocate remaining body buffer\n"
        );
        WRAP(RECEIVE_ERROR, recv(socket, remaining_body, remaining_body_length, 0), "Could not read remaining body\n");
        stringAppend(io_body_builder, LSTRING(remaining_body, remaining_body_length), temp_allocator);
    }
    CATCH(stringBuild(o_body, io_body_builder, allocator), "Could not build request body string\n");

    return SUCCESS;
}

#endif
