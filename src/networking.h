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

// TODO(spencer): this is not a satisfactory solution....
// most likely this is not keeping exact track of different requests and the correctly
// corresponding response, only in the order it is received
typedef struct {
    struct timespec start_time;
    double latency;
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
void send_http_request(request_t request, socket_data_t *socket_data);
void recv_http_response(socket_data_t *timing);

#endif
