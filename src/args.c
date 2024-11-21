#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

static int G_verbose_flag = 0;
// need this because we're allocating that memory
static int G_custom_request_flag = 0;

int verbose_flag() { return G_verbose_flag; }
int custom_request_flag() { return G_custom_request_flag; }

static arg_e arg_type(const char *argument) {
    if (strcmp(argument, "-j") == 0)
        return ARG_N_THREADS;
    else if (strcmp(argument, "-n") == 0)
        return ARG_N_REQUESTS;
    else if (strcmp(argument, "-c") == 0)
        return ARG_N_CONCURRENTS;
    else if (strcmp(argument, "-r") == 0)
        return ARG_CUSTOM_REQUEST;
    else if (strcmp(argument, "-s") == 0)
        return ARG_CUSTOM_HOST;
    else if (strcmp(argument, "-v") == 0)
        return ARG_VERBOSE;

    else
        return -1;
};

void arg_parse(int argc, char **argv, args_t *args) {
    if (argc == 1)
        return;

    for (int i = 1; i < argc; i++) {
        arg_e arg_value = arg_type(argv[i]);
        switch (arg_value) {

        case ARG_N_THREADS:
            if ((i + 1) > argc) {
                fprintf(stderr, "Number of threads unspecified, assuming default\n");
                continue;
            }
            int n_threads = atoi(argv[i + 1]);
            if (n_threads < 1) {
                fprintf(stderr, "Invalid number of threads specified, assuming default\n");
                continue;
            }

            args->n_threads = n_threads;
            break;

        case ARG_N_REQUESTS:
            if ((i + 1) > argc) {
                fprintf(stderr, "Number of requests unspecified, assuming default\n");
                continue;
            }
            int n_requests = atoi(argv[i + 1]);
            if (n_requests < 1) {
                fprintf(stderr, "Invalid number of threads specified, assuming default\n");
                continue;
            }

            args->n_requests = n_requests;
            break;

        case ARG_N_CONCURRENTS:
            if ((i + 1) > argc) {
                fprintf(stderr, "Number of concurrent connections unspecified, assuming default\n");
                continue;
            }
            int n_concurrent = atoi(argv[i + 1]);
            if (n_concurrent < 1) {
                fprintf(stderr, "Invalid number of threads specified, assuming default\n");
                continue;
            }

            args->n_concurrent = n_concurrent;
            break;

        case ARG_CUSTOM_REQUEST:
            if ((i + 1) > argc) {
                fprintf(stderr, "HTTP request unspecified, assuming default\n");
                continue;
            }

            // this allocates
            args->request.content = strdup(argv[i + 1]);
            G_custom_request_flag = 1;
            break;

        case ARG_CUSTOM_HOST:
            if ((i + 1) > argc) {
                fprintf(stderr, "Custom host unspecified, assuming default\n");
                continue;
            }

            char *ip_string = strtok(argv[i + 1], ":");
            if (ip_string == NULL) {
                fprintf(stderr, "Invalid server address format: [ip:port], assuming default\n");
                continue;
            }
            memcpy(args->host.ip, ip_string, sizeof(char) * strlen(ip_string));

            char *port_number_str = strtok(NULL, ":");
            if (port_number_str == NULL) {
                fprintf(stderr, "Invalid server address format: [ip:port], assuming default\n");
                continue;
            }
            args->host.port = atoi(port_number_str);

            break;

        case ARG_VERBOSE:
            G_verbose_flag = 1;
            break;

        case ARG_COUNT:
            fprintf(stderr, "Shouldn't be here");
            break;
        }
    }
};
