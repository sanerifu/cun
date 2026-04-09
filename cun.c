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

typedef enum {
    SUCCESS,
    SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    ACCEPT_ERROR,
} Result;

static int handleRequest(void* socket_ptr) {
    int socket = *(int*)socket_ptr;
    char buf[BUFSIZ];
    int read_size = 0;
    while((read_size = recv(socket, buf, BUFSIZ-1, 0)) > 0) {
        send(socket, buf, read_size, 0);
    }
    close(socket);
    free(socket_ptr);
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
        thrd_create(&thread, &handleRequest, socket_ptr);
    }

    shutdown(server_socket, SHUT_RDWR);
    close(server_socket);
    return SUCCESS;
}

int main() {
    serverLoop();
}
