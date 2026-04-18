#ifndef __RESULT_C__
#define __RESULT_C__

#include <iso646.h>

typedef enum {
    SUCCESS,
    OUT_OF_MEMORY,
    SOCKET_ERROR,
    BIND_ERROR,
    LISTEN_ERROR,
    ACCEPT_ERROR,
    RECEIVE_ERROR,
    SEND_ERROR,
    UNRECOGNIZED_HTTP_METHOD,
    UNSUPPORTED_HTTP_VERSION,
    INVALID_PERCENT_ENCODING
} Result;

#define THROW(result, fmt, ...)                                                                          \
    do {                                                                                                 \
        fprintf(stderr, "%s %s:%d \x1b[41m" #result ": " fmt "\x1b[0m\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
        return result;                                                                                   \
    } while (0)

#define BUBBLE(expr, fmt, ...)                                                               \
    if ((result = expr)) {                                                                  \
        fprintf(stderr, "%s %s:%d \x1b[31m" fmt "\x1b[0m\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
        return result;                                                                      \
    }

#define ASSERT(result, expr, fmt, ...)                                                                   \
    if (not(expr)) {                                                                                     \
        fprintf(stderr, "%s %s:%d \x1b[41m" #result ": " fmt "\x1b[0m\n", __func__, __FILE__, __LINE__, ##__VA_ARGS__); \
        return result;                                                                                   \
    }

#endif
