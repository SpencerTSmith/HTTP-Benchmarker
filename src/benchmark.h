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
    EXT_CODE_COUNT,
} exit_code_e;

void bench_http_request(const args_t *args);
#endif
