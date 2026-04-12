#ifndef __REQUEST_C__
#define __REQUEST_C__

#include <errno.h>
#include <sys/socket.h>

#include "arena.c"
#include "common.c"
#include "result.c"
#include "str.c"
#include "string_builder.c"

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

    char* header = *o_header;
    size_t header_length = *o_header_length;

    char* body = NULL;
    size_t body_length = 0;

    {
        char* line = NULL;
        size_t line_length = 0;
        while ((line = stringSplit(&line_length, &header, &header_length, "\r\n", STRING_NULL_TERMINATED))) {
            size_t key_length = 0;
            char* key = stringSplit(&key_length, &line, &line_length, ":", STRING_NULL_TERMINATED);

            size_t value_length = line_length;
            char* value = line;
            stringTrim(&value, &value_length);

            if (stringCompare(key, key_length, "Content-Length", STRING_NULL_TERMINATED) == 0) {
                char* value_copy = NULL;
                stringDuplicate(&value_copy, value, value_length, &temp_allocator);
                body_length = strtoull(value_copy, NULL, 10);
                break;
            }
        }
    }

    size_t remaining_body_length = body_length - stringBuilderLength(body_builder);
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
