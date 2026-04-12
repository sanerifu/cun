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

    size_t content_length;
};

static Result readRequest(
    char** o_header,
    size_t* o_header_length,
    char** o_body,
    size_t* o_body_length,
    int socket,
    Arena* allocator
) {
    Result result = SUCCESS;
    Arena CLEAN(arenaDestroy) temp_allocator = NULL;
    StringBuilder header_builder = NULL;
    StringBuilder body_builder = NULL;
    size_t initial_body_length = 0;
    {
        char buf[BUFSIZ];
        int read_size = 0;
        while ((read_size = recv(socket, buf, BUFSIZ - 1, 0)) != 0) {
            WRAP(RECEIVE_ERROR, read_size, "Could not receive: %s\n", strerror(errno));
            char* end_position = strstr(buf, "\r\n\r\n");
            if (end_position != NULL) {
                size_t last_header_length = end_position - buf;
                CATCH(
                    stringAppendBuffer(&header_builder, buf, last_header_length, &temp_allocator),
                    "Could not push last part of header"
                );
                CATCH(
                    stringAppendBuffer(
                        &body_builder,
                        end_position + 4,
                        read_size - last_header_length - 4,
                        &temp_allocator
                    ),
                    "Could not push first part of body"
                );
                break;
            }
            CATCH(
                stringAppendBuffer(&header_builder, buf, read_size, &temp_allocator),
                "Could not append the read buffer\n"
            );
        }
    }
    CATCH(
        stringBuild(o_header, o_header_length, &header_builder, allocator),
        "Could not build request header string\n"
    );

    String header = {0};
    String body = {0};

    {
        String line = {0};
        while ((line = stringSplit(&header, STRING_LITERAL("\r\n"))).data) {
            String key = stringSplit(&line, STRING_LITERAL(":"));
            String value = stringTrim(line);

            if (stringCompare(key, STRING_LITERAL("Content-Length")) == 0) {
                String value_copy = {0};
                CATCH(stringDuplicate(&value_copy, value, &temp_allocator), "Could not duplicate content length to null-terminated string");
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
        stringAppendBuffer(&body_builder, remaining_body, remaining_body_length, &temp_allocator);
    }
    CATCH(stringBuild(o_body, o_body_length, &body_builder, allocator), "Could not build request body string\n");

    return SUCCESS;
}

#endif
