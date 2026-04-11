#include <iso646.h>

#include "result.c"
#include "string_builder.c"

#include <stdio.h>
#include <readline/readline.h>

#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <threads.h>
#include "arena.c"

static void freeWrapper(void* data) {
    free(*(void**)data);
}

static void closeWrapper(void* data) {
    close(*(int*)data);
}

static void arenaDestroyWrapper(void* data) {
    arenaDestroy((Arena*)data);
}

#define CLEAN(f) __attribute__((cleanup(f##Wrapper)))

static Result readRequest(char** o_header, size_t* o_header_length, char** o_body, size_t* o_body_length, int socket, Arena* allocator) {
    Result result = SUCCESS;
    Arena CLEAN(arenaDestroy) temp_allocator = NULL;
    StringBuilder header_builder = NULL;
    StringBuilder body_builder = NULL;
    {
        char buf[BUFSIZ];
        int read_size = 0;
        while((read_size = recv(socket, buf, BUFSIZ-1, 0)) != 0) {
            WRAP(RECEIVE_ERROR, read_size, "Could not receive: %s\n", strerror(errno));
            char* end_position = strstr(buf, "\r\n\r\n");
            if(end_position != NULL) {
                size_t last_header_length = end_position - buf;
                CATCH(stringAppendBuffer(&header_builder, buf, last_header_length, &temp_allocator), "Could not push last part of header");
                CATCH(stringAppendBuffer(&body_builder, end_position + 4, read_size - last_header_length - 4, &temp_allocator), "Could not push first part of body");
                break;
            }
            CATCH(stringAppendBuffer(&header_builder, buf, read_size, &temp_allocator), "Could not append the read buffer\n");
        }
    }
    CATCH(stringBuild(o_header, o_header_length, &header_builder, allocator), "Could not build request header string\n");
    CATCH(stringBuild(o_body, o_body_length, &body_builder, allocator), "Could not build request body string\n");

    return SUCCESS;
}

static int requestHandler(void* socket_ptr) {
    void* CLEAN(free) _socket_ptr_copy = socket_ptr;

    Arena allocator = NULL;
    Result result = SUCCESS;
    int socket = *(int*)socket_ptr;
    char* request = NULL;
    size_t length = 0;
    char* body = NULL;
    size_t body_length = 0;

    if((result = readRequest(&request, &length, &body, &body_length, socket, &allocator))) {
        fprintf(stderr, "Could not read request\n");
        return result;
    }

    send(socket, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!", sizeof("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!") - 1, 0);
    return SUCCESS;
}

static int serverLoop(void* sock) {
    int server_socket = *(int*)sock;
    int client_socket = 0;
    struct sockaddr_in address = {0};
    socklen_t address_length = sizeof(address);

    while(true) {
        if(listen(server_socket, 3) < 0) {
            fprintf(stderr, "Listening Error: %s\n", strerror(errno));
            close(server_socket);
            return LISTEN_ERROR;
        }

        if((client_socket = accept(server_socket, (struct sockaddr*)&address, &address_length)) < 0) {
            if(errno == EINVAL) {
                fprintf(stderr, "Closing...\n");
                break;
            }
            fprintf(stderr, "Accepting Error: %d %s\n", errno, strerror(errno));
            // close(server_socket);
            return ACCEPT_ERROR;
        }

        char ip[16];
        inet_ntop(AF_INET, &address, ip, address_length);
        printf("Connected to %s\n", ip);

        thrd_t thread;
        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = client_socket;
        thrd_create(&thread, &requestHandler, socket_ptr);
    }

    close(server_socket);
    return SUCCESS;
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(8080),
    };
    socklen_t address_length = sizeof(address);

    if(server_socket < 0) {
        fprintf(stderr, "Socket Error: %s\n", strerror(errno));
        return SOCKET_ERROR;
    }

    {
        int option = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    }

    if(bind(server_socket, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "Bind Error: %s\n", strerror(errno));
        close(server_socket);
        return BIND_ERROR;
    }

    thrd_t server_thread;
    thrd_create(&server_thread, &serverLoop, &server_socket);

    while(true) {
        char* line = readline("> ");
        if(strcmp(line, "exit") == 0) {
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
