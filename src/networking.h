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

// group latency timings up with sockets
typedef struct {
    int fd;
    int n_requests;
    int current_request;
    struct timespec recent_start_time;
    double *latencies;
} socket_data_t;

// these are async/non-blocking... whatever you wanna call it
// assumes 0 initialized
int get_connected_socket(host_t host);
void send_http_request(request_t request, socket_data_t *socket_data);
void recv_http_response(socket_data_t *timing);

#endif
