#ifndef __RESULT_C__
#define __RESULT_C__

typedef enum {
    SUCCESS,
    OUT_OF_MEMORY,
    SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    ACCEPT_ERROR,
    RECEIVE_ERROR,
    SEND_ERROR
} Result;

#define CATCH(expr, ...) \
    if((result = expr)) { fprintf(stderr, ##__VA_ARGS__); return result; }

#define WRAP(result, expr, ...) if((expr) < 0) { fprintf(stderr, ##__VA_ARGS__); return result; }

#endif
