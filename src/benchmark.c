#define _GNU_SOURCE

#include "benchmark.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "networking.h"

#define MAX_EVENT_N 100

static void *bench_worker(void *args);
typedef struct worker_args_t {
    int n_requests;
    int n_concurrent;
    host_t host;
    request_t request;
    double batch_time;
    socket_data_t *socket_datas;
} worker_args_t;

// kind of large struct so we'll pass by pointer
void bench_http_request(const args_t *args) {
    // just since we access it so much
    int n_threads = args->n_threads;

    // vla, not a fan but i don't like the way malloc looks for threads
    pthread_t workers[n_threads];
    int n_reqs_per_worker = args->n_requests / n_threads;

    worker_args_t worker_args[n_threads];

    for (int i = 0; i < n_threads; i++) {
        worker_args[i] = (worker_args_t){
            .n_requests = n_reqs_per_worker,
            .n_concurrent = args->n_concurrent,
            .host = args->host,
            .request = args->request,
            .batch_time = 0.0,
            .socket_datas = NULL,
        };
        // memory for runtime dependent num sockets
        worker_args[i].socket_datas = calloc(n_reqs_per_worker, sizeof(socket_data_t));
        if (worker_args[i].socket_datas == NULL) {
            fprintf(stderr, "Socket data memory allocation failed");
            exit(EXT_ERROR_SOCKET_DATA_ALLOCATE);
        }

        if (pthread_create(&workers[i], NULL, bench_worker, &worker_args[i]) != 0) {
            perror("Worker thread SPAWN failed");
            exit(EXT_ERROR_THREAD_SPAWN);
        }
    }

    // time taken for workers to finish, not including thread spawning
    struct timespec start_work_time;
    clock_gettime(CLOCK_MONOTONIC, &start_work_time);

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

    struct timespec end_work_time;
    clock_gettime(CLOCK_MONOTONIC, &end_work_time);
    double work_time = (end_work_time.tv_sec - start_work_time.tv_sec) +
                       (end_work_time.tv_nsec - start_work_time.tv_nsec) / 1e9;

    // can free that memory now, if we needed it
    if (custom_request_flag()) {
        free(args->request.content);
    }

    // a little space here, huh
    printf("\n\n\n");

    // print out our results
    for (int i = 0; i < n_threads; i++) {
        printf("Total Work Time (includes socket creation, etc.) : %.9f", work_time);
        printf("Worker %d stats ----\n", i);

        double avg_latency = 0.0;
        for (int j = 0; j < n_reqs_per_worker; j++) {
            avg_latency += worker_args[i].request_timings[j].latency;
        }
        avg_latency /= n_reqs_per_worker;

        double throughput = n_reqs_per_worker / worker_args[i].batch_time;

        printf("	Request batch time : %d requests in %0.9f seconds\n", n_reqs_per_worker,
               worker_args[i].batch_time);
        printf("	Average request latency : %0.9f seconds\n", avg_latency);
        printf("	Request throughput : %0.9f requests/second\n", throughput);

        // done with stats now
        free(worker_args[i].request_timings);
    }
}

static void *bench_worker(void *worker_args) {
    worker_args_t *args = (worker_args_t *)worker_args;

    // async shenanigans oh yeah
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("Epoll creation failed");
        exit(EXT_ERROR_EPOLL_CREATE);
    }

    struct epoll_event events[MAX_EVENT_N];

    int n_sockets_connected = 0;
    int n_reqs_per_socket = args->n_requests / args->n_concurrent;

    // Set up our sockets
    for (int i = 0; i < args->n_concurrent; i++) {
        args->socket_datas[i].fd = get_connected_socket(args->host);

        args->socket_datas[i].current_request = 0;
        args->socket_datas[i].n_requests = n_reqs_per_socket;

        // runtime dependent num of requsts per socket
        args->socket_datas[i].timings = calloc(n_reqs_per_socket, sizeof(request_timing_t));
        if (args->socket_datas[i].timings == NULL) {
            fprintf(stderr, "Timing data memory allocation failed");
            exit(EXT_ERROR_TIMING_DATA_ALLOCATE);
        }

        // and an event for every socket
        struct epoll_event event = {0};
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR;
        event.data.fd = args->socket_datas[i].fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, args->socket_datas[i].fd, &event) < 0) {
            perror("Epoll event for socket failed to be added");
            exit(EXT_ERROR_EPOLL_ADD_EVENT);
        }

        // initial to get the event loop started
        send_http_request(args->request, &args->socket_datas[i]);
        args->socket_datas->current_request++;
        n_sockets_connected++;
    }

    // timing for batch of requests
    struct timespec start_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &start_batch_time);

    // event loop, lets get nested
    while (n_sockets_connected > 0) {
        int n_events = epoll_wait(epoll_fd, events, MAX_EVENT_N, -1);
        if (n_events < 0) {
            perror("Epoll wait failed");
            exit(EXT_ERROR_EPOLL_WAIT);
        }

        for (int e = 0; e < n_events; e++) {
            int event_socket_fd = events[e].data.fd;

            if (events[e].events & EPOLLERR) {
                perror("Epoll socket event failed, closing that socket");
                close(event_socket_fd);
                n_sockets_connected--;

                // we are recieving a response
            } else if (events[e].events & EPOLLIN) {
                // find the socket we got an event for...
                // TODO(spencer): this might benefit from hashing
                for (int i = 0; i < args->n_concurrent; i++) {
                    if (args->socket_datas[i].fd == event_socket_fd) {
                        // this will also calc the timing
                        recv_http_response(&args->socket_datas[i]);

                        // if we can send a request
                        if (args->socket_datas[i].current_request <
                            args->socket_datas[i].n_requests) {
                            // this will also calc the timing
                            send_http_request(args->request, &args->socket_datas[i]);

                            // I think I like this decrement here (not in the send_http func) so
                            // it's more obvious what its doing
                            args->socket_datas[i].current_request++;
                        } else {
                            close(event_socket_fd);
                            n_sockets_connected--;
                        }

                        // don't have to go through the rest of the socket_fds
                        break;
                    }
                }
                // we have the ability to send a message
            } else if (events[e].events & EPOLLOUT) {
                for (int i = 0; i < args->n_concurrent; i++) {

                    // we have this events socket and we still have more requests to send
                    if (args->socket_datas[i].fd == event_socket_fd &&
                        args->socket_datas[i].current_request < args->socket_datas[i].n_requests) {
                        // this will also calc the timing
                        send_http_request(args->request, &args->socket_datas[i]);

                        // I think I like this decrement here (not in the send_http func) so it's
                        // more obvious what its doing
                        args->socket_datas[i].current_request++;

                        // don't have to go through the rest of the socket_fds
                        break;
                    }
                }
            }
        }
    }

    struct timespec end_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &end_batch_time);
    args->batch_time = (end_batch_time.tv_sec - start_batch_time.tv_sec) +
                       (end_batch_time.tv_nsec - start_batch_time.tv_nsec) / 1e9;

    // close(socket_fd);
    return NULL;
}
