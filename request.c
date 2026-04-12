#ifndef __REQUEST_C__
#define __REQUEST_C__

#include <errno.h>
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

typedef struct RequestHeader RequestHeader;
struct RequestHeader {
    RequestMethod method;
    String user_agent;
    size_t content_length;
};

static Result readRequest(String* o_header, String* o_body, int socket, Arena* allocator) {
    Result result = SUCCESS;
    Arena CLEAN(arenaDestroy) temp_allocator = NULL;
    StringBuilder header_builder = NULL;
    StringBuilder body_builder = NULL;
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
                    stringAppend(&header_builder, LSTRING(buffer.data, last_header_length), &temp_allocator),
                    "Could not push last part of header"
                );
                CATCH(
                    stringAppend(
                        &body_builder,
                        LSTRING(end_position + 4, read_size - last_header_length - 4),
                        &temp_allocator
                    ),
                    "Could not push first part of body"
                );
                break;
            }
            CATCH(
                stringAppend(&header_builder, LSTRING(buffer.data, read_size), &temp_allocator),
                "Could not append the read buffer\n"
            );
        }
    }

    String header = {0};
    String body = {0};

    CATCH(stringBuild(&header, &header_builder, allocator), "Could not build request header string\n");

    String header_copy = header;

    {
        String line = {0};
        while ((line = stringSplit(&header_copy, STRING_LITERAL("\r\n"))).data) {
            String key = stringSplit(&line, STRING_LITERAL(":"));
            String value = stringTrim(line);

            if (stringCompare(key, STRING_LITERAL("Content-Length")) == 0) {
                String value_copy = {0};
                CATCH(
                    stringDuplicate(&value_copy, value, &temp_allocator),
                    "Could not duplicate content length to null-terminated string"
                );
                body.length = strtoull(value_copy.data, NULL, 10);
                break;
            }
        }
    }

    size_t remaining_body_length = body.length - stringBuilderLength(body_builder);
    if (remaining_body_length > 0) {
        char* remaining_body = NULL;
        CATCH(
            arenaAllocate(&remaining_body, &temp_allocator, remaining_body_length),
            "Could not allocate remaining body buffer\n"
        );
        WRAP(RECEIVE_ERROR, recv(socket, remaining_body, remaining_body_length, 0), "Could not read remaining body\n");
        stringAppend(&body_builder, LSTRING(remaining_body, remaining_body_length), &temp_allocator);
    }
    CATCH(stringBuild(&body, &body_builder, allocator), "Could not build request body string\n");

    *o_header = header;
    *o_body = body;

    return SUCCESS;
}

static Result parseRequestHeader(RequestHeader* o_ret, String header) {
    Result result = SUCCESS;
    String line = {0};
    RequestHeader ret = {0};
    Arena CLEAN(arenaDestroy) temp_allocator = NULL;

    String http_start = stringSplit(&header, STRING_LITERAL("\r\n"));
    String method = stringSplit(&http_start, STRING_LITERAL(" "));
    if(stringCompare(method, STRING_LITERAL("GET")) == 0) {
        ret.method = GET_REQUEST;
    } else if(stringCompare(method, STRING_LITERAL("POST")) == 0) {
        ret.method = POST_REQUEST;
    } else if(stringCompare(method, STRING_LITERAL("HEAD")) == 0) {
        ret.method = HEAD_REQUEST;
    } else if(stringCompare(method, STRING_LITERAL("PUT")) == 0) {
        ret.method = PUT_REQUEST;
    } else if(stringCompare(method, STRING_LITERAL("PATCH")) == 0) {
        ret.method = PATCH_REQUEST;
    } else if(stringCompare(method, STRING_LITERAL("DELETE")) == 0) {
        ret.method = DELETE_REQUEST;
    } else {
        fprintf(stderr, "Unrecognized HTTP method: \"%.*s\"\n", FORMAT(method));
        return UNRECOGNIZED_HTTP_METHOD;
    }

    while((line = stringSplit(&header, STRING_LITERAL("\r\n"))).data) {
        String key = stringSplit(&line, STRING_LITERAL(":"));
        String value = line;

        if(stringCompare(key, STRING_LITERAL("Content-Length")) == 0) {
            String value_copy = {0};
            CATCH(stringDuplicate(&value_copy, value, &temp_allocator), "Could not allocate null-terminated string for content length");
            ret.content_length = strtoull(value_copy.data, NULL, 10);
        }
    }

    *o_ret = ret;
}

#endif
