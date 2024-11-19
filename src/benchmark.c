#include "benchmark.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int n_requests;
    host_t host;
    request_t request;
    double batch_time;
} worker_args_t;

void *bench_worker(void *worker_args) {
    worker_args_t *args = (worker_args_t *)worker_args;

    // get socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        exit(EXT_ERR_SOCK_CREATE);
    }

    // define socket address
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(args->host.port), // convert to how the network defines port #
    };

    // convert to binary format of ip address, probably not nessecary if just connecting to your own
    // machine
    if (inet_pton(AF_INET, args->host.ip, &server_address.sin_addr) <= 0) {
        perror("Conversion of server address failed");
        exit(EXT_ERR_SERVER_ADDRESS);
    }

    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address))) {
        perror("Connection failed");
        close(socket_fd);
        exit(EXT_ERR_CONNECTION);
    }

    struct timespec start_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &start_batch_time);

    for (int i = 0; i < args->n_requests; i++) {
        if (send(socket_fd, args->request.content, args->request.length, 0) < 0) {
            perror("Request send failed");
            close(socket_fd);
            exit(EXT_ERR_REQUEST_FAIL);
        }

        char response[4096] = {0};
        int n_bytes_response = recv(socket_fd, response, 4096 - 1, 0);

        if (n_bytes_response < 0) {
            perror("Recieve response failed");
        } else {
            response[n_bytes_response] = '\0'; // terminating character
            printf("Response received: %s\n", response);
        }
    }

    struct timespec end_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &end_batch_time);
    args->batch_time = end_batch_time.tv_sec - start_batch_time.tv_sec;

    close(socket_fd);
    return NULL;
}

void bench_http_request(const args_t *args) {
    // no reason to spawn thread
    if (args->n_threads == 0) {
        // this looks ugly sorry
        bench_worker(&(worker_args_t){
            .n_requests = args->n_requests,
            .host = args->host,
            .request = args->request,
            .batch_time = 0,
        });
        return;
    }

    // vla
    pthread_t workers[args->n_threads];
    int n_reqs_per_worker = args->n_requests / args->n_threads;

    worker_args_t worker_args = {
        .n_requests = n_reqs_per_worker,
        .host = args->host,
        .request = args->request,
        .batch_time = 0,
    };

    for (int i = 0; i < args->n_threads; i++) {
        if (pthread_create(&workers[i], NULL, &bench_worker, &worker_args) != 0) {
            perror("Worker thread spawn failed");
        }
    }

    for (int i = 0; i < args->n_threads; i++) {
        pthread_join(workers[i], NULL);
    }
}
