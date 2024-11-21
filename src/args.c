#include "args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int verbose_flag = 0;

int verbose() { return verbose_flag; }

static arg_e arg_type(const char *argument) {
    if (strcmp(argument, "-j") == 0)
        return ARG_N_THREADS;
    else if (strcmp(argument, "-n") == 0)
        return ARG_N_REQUESTS;
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
    if (argc < 3)
        return;

    for (int i = 1; i < argc; i++) {
        arg_e arg_value = arg_type(argv[i]);
        switch (arg_value) {

        case ARG_N_THREADS:
            if ((i + 1) < argc) {
                args->n_threads = atoi(argv[i + 1]);
            } else {
                fprintf(stderr, "Number of threads unspecified, assuming default\n");
            }
            break;

        case ARG_N_REQUESTS:
            if ((i + 1) < argc) {
                args->n_requests = atoi(argv[i + 1]);
            } else {
                fprintf(stderr, "Number of requests unspecified, assuming default\n");
            }
            break;

        // TODO(spencer): set this up
        case ARG_CUSTOM_REQUEST:

            break;

        case ARG_CUSTOM_HOST:
            if ((i + 1) < argc) {
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
            }

            break;

        case ARG_VERBOSE:
            verbose_flag = 1;
            break;

        case ARG_COUNT:
            fprintf(stderr, "Shouldn't be here");
            break;
        }
    }
};
