#ifndef ARG_H
#define ARG_H

typedef enum {
    ARG_N_THREADS,
    ARG_N_REQUESTS,
    ARG_CUSTOM_REQUEST,
    ARG_CUSTOM_HOST,
    ARG_VERBOSE,
    ARG_COUNT,
} arg_e;

typedef struct {
    char ip[16];
    int port;
} host_t;

typedef struct {
    // this maybe should be dynamic
    char *content;
    int length;
} request_t;

typedef struct {
    int n_threads;
    int n_requests;
    host_t host;
    request_t request;
} args_t;

void arg_parse(int argc, char **argv, args_t *args);

int verbose_flag();
int custom_request_flag();

#endif
