#ifndef ARG_H
#define ARG_H

#include <stdio.h>

#include "networking.h"

typedef enum {
    ARG_N_THREADS,
    ARG_N_REQUESTS,
    ARG_N_CONCURRENTS,
    ARG_CUSTOM_REQUEST,
    ARG_CUSTOM_HOST,
    ARG_VERBOSE,
    ARG_OUT_FILE,
    ARG_COUNT,
} arg_e;

typedef struct {
    int n_threads;
    int n_requests;
    int n_concurrent;
    host_t host;
    request_t request;
    FILE *out_file;
} args_t;

void arg_parse(int argc, char **argv, args_t *args);

int verbose_flag();
int custom_request_flag();

#endif
