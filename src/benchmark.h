#ifndef BENCHMARK_H
#define BENCHMARK_H

#include "args.h"

typedef enum {
    EXT_SUCESS,
    EXT_ERR_SOCK_CREATE,
    EXT_ERR_SERVER_ADDRESS,
    EXT_ERR_CONNECTION,
    EXT_ERR_REQUEST_FAIL,
    NUM_EXT_CODE,
} exit_code_e;

void bench_http_request(const args_t *args);
#endif
