#include "args.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static arg_e arg_type(const char *argument) {
    if (strcmp(argument, "-j")) {
        return ARG_N_THREADS;
    } else if (strcmp(argument, "-n")) {
        return ARG_N_REQUESTS;
    } else if (strcmp(argument, "-r")) {
        return ARG_CUSTOM_REQUEST;
    }

    else {
        return -1;
    }
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
                i++; // can skip the next one
            } else {
                fprintf(stderr, "Number of threads unspecified");
            }
            break;

        case ARG_N_REQUESTS:
            if ((i + 1) < argc) {
                args->n_requests = atoi(argv[i + 1]);
                i++; // can skip the next one
            } else {
                fprintf(stderr, "Number of requests unspecified");
            }
            break;

        // TODO(spencer): set this up
        case ARG_CUSTOM_REQUEST:

            break;

        // TODO(spencer): set this up
        case ARG_CUSTOM_HOST:

            break;

        case ARG_COUNT:
            fprintf(stderr, "Shouldn't be here");
            break;
        }
    }
};
