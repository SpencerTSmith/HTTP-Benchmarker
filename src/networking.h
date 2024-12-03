#ifndef NETWORKING_H
#define NETWORKING_H

#include <time.h>

typedef struct {
    char ip[16];
    int port;
} host_t;

typedef struct {
    char *content;
    int length;
} request_t;

// apparently, http 1.1 ensures that requests sent over the same socket, are responded to in the
// same order they are received. So we can take advantage of this and not worry about tying a unique
// id to each request timing.
typedef struct {
    struct timespec send_time;
    struct timespec recv_time;
} request_timing_t;

// group latency timings up with sockets
typedef struct {
    int fd;
    int n_requests;
    int current_request;
    int current_response;
    request_timing_t *timings;
} socket_data_t;

// these are async/non-blocking... whatever you wanna call it
// assumes 0 initialized
int get_connected_socket(host_t host);
void send_http_request(int socket_fd, request_t request);
// for now just truncate the response, presumably if we are benchmarking we don't need to receive
// the full response
void recv_http_response(int socket_fd, char *buffer, size_t buffer_size);

#endif
