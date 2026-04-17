#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iso646.h>
#include <lua5.1/lauxlib.h>
#include <lua5.1/lua.h>
#include <netinet/in.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <threads.h>
#include <unistd.h>

#include "arena.c"
#include "request.c"
#include "result.c"

extern void luaL_openlibs(lua_State* L);  // my headers don't have this for some reason

static void lua_closeWrapper(void* data) {
    lua_close(*(lua_State**)data);
}

static char const* requestMethodToString(RequestMethod method) {
    switch (method) {
        case GET_REQUEST:
            return "get";
        case POST_REQUEST:
            return "post";
        case HEAD_REQUEST:
            return "head";
        case PUT_REQUEST:
            return "put";
        case PATCH_REQUEST:
            return "patch";
        case DELETE_REQUEST:
            return "delete";
    }
}

static char const* responsePhrase(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 404:
            return "Not Found";
        default:
            return "CODE";
    }
}

typedef struct RequestHandlerInput RequestHandlerInput;
struct RequestHandlerInput {
    int socket;
    char ip[16];
};

static int requestHandler(void* input_ptr) {
    void* CLEAN(free) _socket_ptr_copy = input_ptr;

    RequestHandlerInput input = *(RequestHandlerInput*)input_ptr;

    Arena allocator = {0};
    Result result = SUCCESS;
    int socket = input.socket;
    String ip = ZSTRING(input.ip);

    String header = {0};
    String body = {0};

    RequestHeader parsed_header = {0};

    {
        StringBuilder body_builder = {0};
        Arena CLEAN(arenaDestroy) temp_allocator = {0};
        CATCH(
            readRequestHeader(&header, &body_builder, socket, &allocator, &temp_allocator),
            "Could not read request header\n"
        );
        CATCH(parseRequestHeader(&parsed_header, header, &allocator), "Could not parse request header\n");
        CATCH(
            readRequestBody(&body, &body_builder, socket, &parsed_header, &allocator, &temp_allocator),
            "Could not read request body\n"
        );
    }

    String formatted;

    String lua_content_path;
    CATCH(
        stringFormat(&lua_content_path, &allocator, ".%.*s.lua", FORMAT(parsed_header.path)),
        "Could not create path"
    );

    FILE* CLEAN(fclose) fp = NULL;
    String lua_data = LSTRING(NULL, 0);

    if ((fp = fopen(lua_content_path.data, "rb"))) {
        CATCH(stringFromFile(&lua_data, &allocator, fp), "Could not read from file\n");
    }

    if (fp != NULL) {
        lua_State* CLEAN(lua_close) L = luaL_newstate();
        luaL_openlibs(L);

        {
            lua_newtable(L);

            lua_pushlstring(L, ip.data, ip.length);
            lua_setfield(L, -2, "ip");

            lua_pushstring(L, requestMethodToString(parsed_header.method));
            lua_setfield(L, -2, "method");

            lua_pushlstring(L, parsed_header.path.data, parsed_header.path.length);
            lua_setfield(L, -2, "path");

            lua_pushlstring(L, parsed_header.user_agent.data, parsed_header.user_agent.length);
            lua_setfield(L, -2, "user_agent");

            lua_pushlstring(L, body.data, body.length);
            lua_setfield(L, -2, "body");

            {
                lua_newtable(L);
                String queries = parsed_header.queries;
                String query;
                while((query = stringSplit(&queries, STRING_LITERAL(";"))).data) {
                    String key = stringSplit(&query, STRING_LITERAL("="));
                    String value = query;
                    String decoded_value;
                    CATCH(stringUrlDecode(&decoded_value, value, &allocator), "Could not decode URL query\n");
                    lua_pushlstring(L, key.data, key.length);
                    lua_pushlstring(L, decoded_value.data, decoded_value.length);
                    lua_rawset(L, -3);
                }
                lua_setfield(L, -2, "queries");
            }

            lua_setglobal(L, "request");
        }

        luaL_dostring(L, lua_data.data);

        {
            lua_getfield(L, -1, "status");
            int status_code = luaL_optint(L, -1, 200);
            lua_pop(L, 1);

            lua_getfield(L, -1, "content_type");
            char const* content_type = luaL_optstring(L, -1, "text/html");
            lua_pop(L, 1);

            String response_body = {0};
            lua_getfield(L, -1, "body");
            response_body.data = (char*)luaL_optlstring(L, -1, "", &response_body.length);
            lua_pop(L, 1);

            String cookies = {0};
            StringBuilder cookie_builder = {0};
            lua_getfield(L, -1, "cookies");
            lua_pushnil(L);
            Arena CLEAN(arenaDestroy) temp_allocator = {0};
            while(lua_next(L, -2) != 0) {
                lua_pushvalue(L, -2); // prevent key from being changed by lua_to(l)string
                stringAppendf(&cookie_builder, &temp_allocator, "Set-Cookie: %s=%s\r\n", lua_tostring(L, -1), lua_tostring(L, -2));
                lua_pop(L, 2);
            }
            stringBuild(&cookies, &cookie_builder, &temp_allocator);

            CATCH(
                stringFormat(
                    &formatted,
                    &allocator,
                    "HTTP/1.1 %3d %s\r\n"
                    "Content-Length: %d\r\n"
                    "Content-Type: %s\r\n"
                    "%.*s"
                    "\r\n"
                    "%.*s",
                    status_code,
                    responsePhrase(status_code),
                    response_body.length,
                    content_type,
                    FORMAT(cookies),

                    FORMAT(response_body)
                ),
                "Could not allocate formatted string"
            );
        }

        send(socket, formatted.data, formatted.length, 0);
    } else {
        char error_page[] =
            "<!DOCTYPE html>\n"
            "<html>\n"
            "<head>\n"
            "   <meta charset=\"utf-8\" />\n"
            "   <title>Page not found</title>\n"
            "</head>\n"
            "<body>\n"
            "   <h1>Requested page not found</h1>\n"
            "</body>\n"
            "</html>\n";
        CATCH(
            stringFormat(
                &formatted,
                &allocator,
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Length: %zu\r\n"
                "Content-Type: text/html\r\n"
                "\r\n"
                "%s",

                sizeof(error_page) - 1,
                error_page
            ),
            "Could not allocate formatted string"
        );
        send(socket, formatted.data, formatted.length, 0);
    }

    shutdown(socket, SHUT_RDWR);

    return SUCCESS;
}

static int serverLoop(void* sock) {
    int server_socket = *(int*)sock;
    int client_socket = 0;
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof(address);

    while (true) {
        WRAP(LISTEN_ERROR, listen(server_socket, 3), "Listening Error: %s\n", strerror(errno));

        if ((client_socket = accept(server_socket, (struct sockaddr*)&address, &address_length)) < 0) {
            if (errno == EINVAL) {
                fprintf(stderr, "Closing...\n");
                break;
            }
            fprintf(stderr, "Accepting Error: %d %s\n", errno, strerror(errno));
            return ACCEPT_ERROR;
        }

        char ip[16];
        inet_ntop(AF_INET, &address, ip, address_length);
        printf("Connected to %s\n", ip);

        thrd_t thread;
        RequestHandlerInput* input = malloc(sizeof(*input));
        input->socket = client_socket;
        memcpy(input->ip, ip, sizeof(ip));
        thrd_create(&thread, &requestHandler, input);
    }

    close(server_socket);
    return SUCCESS;
}

int main() {
    int CLEAN(close) server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8080),
    };
    socklen_t address_length = sizeof(address);

    WRAP(SOCKET_ERROR, server_socket, "Socket Error: %s\n", strerror(errno));

    {
        int option = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    }

    if (bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Bind Error: %s\n", strerror(errno));
        close(server_socket);
        return BIND_ERROR;
    }

    thrd_t server_thread;
    thrd_create(&server_thread, &serverLoop, &server_socket);

    while (true) {
        char* line = readline("> ");
        if (line == NULL or strcmp(line, "exit") == 0) {
            fprintf(stderr, "Shutting down the server\n");
            free(line);
            break;
        } else {
            fprintf(stderr, "Unrecognized command: \"%s\"\n", line);
        }
        free(line);
    }

    shutdown(server_socket, SHUT_RDWR);

    int server_result;
    thrd_join(server_thread, &server_result);

    return server_result;
}
