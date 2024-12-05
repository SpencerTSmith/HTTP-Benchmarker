#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "args.h"

static int G_verbose_flag = 0;
// need this because we're allocating that memory
static int G_custom_request_flag = 0;

int verbose_flag() { return G_verbose_flag; }
int custom_request_flag() { return G_custom_request_flag; }

static int handle_custom_request(args_t *args, const char *arg_value);

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
    else if (strcmp(argument, "-f") == 0)
        return ARG_OUT_FILE;

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

            handle_custom_request(args, argv[i + 1]);
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

        case ARG_OUT_FILE:
            if ((i + 1) > argc) {
                fprintf(stderr, "Number of concurrent connections unspecified, assuming default\n");
                continue;
            }

            args->out_file = fopen(argv[i + 1], "w");
            if (args->out_file == NULL) {
                perror("Failed to open file for stdout redirection");
            }

            break;

        case ARG_COUNT:
            fprintf(stderr, "Shouldn't be here");
            break;
        }
    }
};

static int handle_custom_request(args_t *args, const char *arg_value) {
    const char *host_header = "Host:";
    char *host_start = strcasestr(arg_value, host_header);
    if (host_header == NULL) {
        fprintf(stderr, "HTTP host unspecified in request, assuming default\n");
        return -1;
    }

    // little bit of pointer arithmetic find the real start of the substring
    host_start += strlen("Host:");

    while (isspace(*host_start))
        host_start++;

    char *delimiter = strchr(host_start, ':');
    if (delimiter == NULL) {
        fprintf(stderr, "HTTP host format incorrect [ip:port], assuming default\n");
        return -1;
    }
    int ip_length = delimiter - host_start;
    memcpy(args->host.ip, host_start, ip_length);

    char *first_port_digit = delimiter++;
    char *eol = strstr(first_port_digit, "\r\n");
    if (eol == NULL) {
        fprintf(stderr, "HTTP host format incorrect [ip:port], assuming default\n");
        return -1;
    }
    int port_length = eol - first_port_digit;

    char *temp_port_string = malloc((sizeof(char) * port_length) + 1);
    memcpy(temp_port_string, delimiter, port_length);
    temp_port_string[port_length] = '\0';

    args->host.port = atoi(temp_port_string);

    // this allocates, so set a flag to remember to cleanup later
    args->request.content = strdup(arg_value);
    args->request.length = strlen(args->request.content);
    G_custom_request_flag = 1;

    return 1;
}
