#define _GNU_SOURCE

#include "benchmark.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "args.h"

typedef struct {
    int n_requests;
    host_t host;
    request_t request;
    double batch_time;
    double *request_latencies;
} worker_args_t;

static void *bench_worker(void *worker_args) {
    worker_args_t *args = (worker_args_t *)worker_args;

    // get socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        exit(EXT_ERROR_SOCKET_CREATE);
    }

    // define socket address
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(args->host.port), // convert to how the network defines port #
    };

    // convert to binary format of ip address
    if (inet_pton(AF_INET, args->host.ip, &server_address.sin_addr) <= 0) {
        perror("Conversion of server address failed");
        exit(EXT_ERROR_SERVER_ADDRESS);
    }

    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address))) {
        perror("Connection failed");
        close(socket_fd);
        exit(EXT_ERROR_CONNECTION);
    }

    // timing for batch of requests
    struct timespec start_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &start_batch_time);

    for (int i = 0; i < args->n_requests; i++) {
        // time the request latency
        struct timespec start_request_time;
        clock_gettime(CLOCK_MONOTONIC, &start_request_time);

        if (send(socket_fd, args->request.content, args->request.length, 0) < 0) {
            perror("Request send failed");
            close(socket_fd);
            exit(EXT_ERROR_REQUEST_FAIL);
        }

        char response[4096] = {0};
        int n_bytes_response = recv(socket_fd, response, 4096 - 1, 0);
        if (n_bytes_response < 0) {
            perror("Receive response failed");
            close(socket_fd);
            exit(EXT_ERROR_RECEIVE_FAIL);
        }

        if (verbose_flag()) {
            response[n_bytes_response] = '\0';
            printf("Response:\n%s", response);
        }

        struct timespec end_request_time;
        clock_gettime(CLOCK_MONOTONIC, &end_request_time);

        args->request_latencies[i] = (end_request_time.tv_sec - start_request_time.tv_sec) +
                                     (end_request_time.tv_nsec - start_request_time.tv_nsec) / 1e9;
    }

    struct timespec end_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &end_batch_time);
    args->batch_time = (end_batch_time.tv_sec - start_batch_time.tv_sec) +
                       (end_batch_time.tv_nsec - start_batch_time.tv_nsec) / 1e9;

    close(socket_fd);
    return NULL;
}

void bench_http_request(const args_t *args) {
    // no reason to spawn thread
    if (args->n_threads < 2) {
        // this looks ugly sorry
        printf("Beginning requests\n");
        bench_worker(&(worker_args_t){
            .n_requests = args->n_requests,
            .host = args->host,
            .request = args->request,
            .batch_time = 0.0,
        });
        return;
    }

    // just since we access it so much
    int n_threads = args->n_threads;

    // vla, not a fan but i don't like the way malloc looks for threads
    pthread_t workers[n_threads];
    int n_reqs_per_worker = args->n_requests / n_threads;

    worker_args_t worker_args[n_threads];

    for (int i = 0; i < n_threads; i++) {
        worker_args[i] = (worker_args_t){
            .n_requests = n_reqs_per_worker,
            .host = args->host,
            .request = args->request,
            .batch_time = 0.0,
            .request_latencies = NULL,
        };
        worker_args[i].request_latencies = calloc(n_reqs_per_worker, sizeof(double));

        if (pthread_create(&workers[i], NULL, bench_worker, &worker_args[i]) != 0) {
            perror("Worker thread SPAWN failed");
            exit(EXT_ERROR_THREAD_SPAWN);
        }
    }

    int n_threads_done = 0;
    while (n_threads_done < n_threads) {
        n_threads_done = 0;

        // check if threads are done
        for (int i = 0; i < n_threads; i++) {
            if (pthread_tryjoin_np(workers[i], NULL) == 0) {
                printf("A worker has finished\n");
                n_threads_done++;
            }
        }

        if (n_threads_done < n_threads) {
            printf("Waiting for workers to finish...\n");
            sleep(3);
        }
    }

    // can free that memory now, if we needed it
    if (custom_request_flag()) {
        free(args->request.content);
    }

    // a little space here, huh
    printf("\n\n\n");

    // print out our results
    for (int i = 0; i < n_threads; i++) {
        printf("Worker %d stats ----\n", i);

        double avg_latency = 0.0;
        for (int j = 0; j < n_reqs_per_worker; j++) {
            avg_latency += worker_args[i].request_latencies[j];
        }
        avg_latency /= n_reqs_per_worker;

        double throughput = n_reqs_per_worker / worker_args[i].batch_time;

        printf("	Request batch time : %d requests in %0.9f seconds\n", n_reqs_per_worker,
               worker_args[i].batch_time);
        printf("	Average request latency : %0.9f seconds\n", avg_latency);
        printf("	Request throughput : %0.9f requests/second\n", throughput);

        // done with stats now
        free(worker_args[i].request_latencies);
    }
}
