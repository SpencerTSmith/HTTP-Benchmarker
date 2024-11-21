#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "benchmark.h"
#include "networking.h"

void send_http_request(request_t request, socket_data_t *socket_data) {
    if (send(socket_data->fd, request.content, request.length, 0) < 0) {
        perror("Request send failed");
        close(socket_data->fd);
        exit(EXT_ERROR_REQUEST_FAIL);
    }

    if (verbose_flag()) {
        printf("Sent request %d on socket %d\n", socket_data->current_request, socket_data->fd);
    }

    // hmm should this be before we send the request... or after?
    clock_gettime(CLOCK_MONOTONIC, &socket_data->timings[socket_data->current_request].start_time);
}

void recv_http_response(socket_data_t *socket_data) {
    // really don't want to be malloc'ing on every request response so we'll just have to truncate
    char response[4096] = {0};
    int n_bytes_response = recv(socket_data->fd, response, sizeof(response) - 1, 0);
    if (n_bytes_response < 0) {
        perror("Receive response failed");
        close(socket_data->fd);
        exit(EXT_ERROR_RECEIVE_FAIL);
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);

    double latency =
        (end_time.tv_sec - socket_data->timings[socket_data->current_response].start_time.tv_sec) +
        (end_time.tv_nsec -
         socket_data->timings[socket_data->current_response].start_time.tv_nsec) /
            1e9;

    socket_data->timings[socket_data->current_response].latency = latency;

    if (verbose_flag()) {
        response[n_bytes_response] = '\0';
        printf("\nResponse:\n%s", response);
    }
}

int get_connected_socket(host_t host) {
    // define socket address
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(host.port), // convert to how the network defines port #
        .sin_addr = {0},
        .sin_zero = {0},
    };

    // convert to binary format of ip address
    if (inet_pton(AF_INET, host.ip, &server_address.sin_addr) <= 0) {
        perror("Conversion of server address failed");
        exit(EXT_ERROR_SERVER_ADDRESS);
    }

    // get socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        exit(EXT_ERROR_SOCKET_CREATE);
    }

    // flags for socket
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("Getting socket flags failed");
        exit(EXT_ERROR_SOCKET_FLAGS);
    }

    // set socket as non blocking
    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("Setting socket non-blocking failed");
        exit(EXT_ERROR_SOCKET_NON_BLOCK);
    }

    // finally connect
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address))) {
        // make sure we are connecting, above should return immediately
        if (errno != EINPROGRESS) {
            perror("Connection failed");
            close(socket_fd);
            exit(EXT_ERROR_CONNECTION);
        }
    }

    return socket_fd;
}
