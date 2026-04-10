#include "result.c"
#include "string_builder.c"

#include <stdio.h>

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

static Result readRequest(char** o_request, size_t* o_length, int socket, Arena* allocator) {
    Result result = SUCCESS;
    Arena CLEAN(arenaDestroy) temp_allocator = NULL;
    StringBuilder request_builder = NULL;
    {
        char buf[BUFSIZ];
        int read_size = 0;
        while((read_size = recv(socket, buf, BUFSIZ-1, 0)) != 0) {
            if(read_size < 0) {
                fprintf(stderr, "Could not receive: %s\n", strerror(errno));
                return RECEIVE_ERROR;
            }
            if((result = stringAppendBuffer(&request_builder, buf, read_size, &temp_allocator))) {
                fprintf(stderr, "Could not append the read buffer\n");
                return result;
            }
            if(strstr(buf, "\r\n\r\n") != NULL) {
                break;
            }
        }
    }
    if((result = stringBuild(o_request, o_length, &request_builder, allocator))) {
        fprintf(stderr, "Could not build request string\n");
        return result;
    }
    return SUCCESS;
}

static int requestHandler(void* socket_ptr) {
    void* CLEAN(free) _socket_ptr_copy = socket_ptr;

    Arena allocator = NULL;
    Result result = SUCCESS;
    int socket = *(int*)socket_ptr;
    char* request = NULL;
    size_t length = 0;

    if((result = readRequest(&request, &length, socket, &allocator))) {
        fprintf(stderr, "Could not read request\n");
        return result;
    }

    send(socket, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!", sizeof("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 13\r\n\r\nHello, World!") - 1, 0);
    return SUCCESS;
}

static Result serverLoop() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int client_socket = 0;
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

    while(true) {
        if(listen(server_socket, 3) < 0) {
            fprintf(stderr, "Listening Error: %s\n", strerror(errno));
            close(server_socket);
            return LISTEN_ERROR;
        }

        if((client_socket = accept(server_socket, (struct sockaddr*)&address, &address_length)) < 0) {
            fprintf(stderr, "Accepting Error: %s\n", strerror(errno));
            close(server_socket);
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

    shutdown(server_socket, SHUT_RDWR);
    close(server_socket);
    return SUCCESS;
}

int main() {
    serverLoop();
}
