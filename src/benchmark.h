#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "args.h"

typedef enum {
    EXT_SUCESS,
    EXT_ERROR_SOCKET_CREATE,
    EXT_ERROR_SERVER_ADDRESS,
    EXT_ERROR_CONNECTION,
    EXT_ERROR_REQUEST_FAIL,
    EXT_ERROR_RECEIVE_FAIL,
    EXT_ERROR_THREAD_SPAWN,
    EXT_ERROR_THREAD_JOIN,
    EXT_ERROR_SOCKET_FLAGS,
    EXT_ERROR_SOCKET_NON_BLOCK,
    EXT_ERROR_SOCKET_DATA_ALLOCATE,
    EXT_ERROR_TIMING_DATA_ALLOCATE,
    EXT_ERROR_EPOLL_CREATE,
    EXT_ERROR_EPOLL_ADD_EVENT,
    EXT_ERROR_EPOLL_WAIT,
    EXT_CODE_COUNT,
} exit_code_e;

void bench_http_request(const args_t *args);
#endif
