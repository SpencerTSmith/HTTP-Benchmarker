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

// this frees stats held in worker args
static void calc_print_stats(const args_t *args, const worker_args_t *worker_datas, int n_workers,
                             double work_time);

// kind of large struct so we'll pass by pointer
void bench_http_request(const args_t *args) {
    // just since we access it so much
    int n_threads = args->n_threads;

    pthread_t *workers = calloc(n_threads, sizeof(pthread_t));
    if (workers == NULL) {
        fprintf(stderr, "Thread id memory allocation failed");
        exit(EXT_ERROR_THREAD_ID_ALLOCATE);
    }

    worker_args_t *worker_args = calloc(n_threads, sizeof(worker_args_t));
    if (workers == NULL) {
        fprintf(stderr, "Thread args memory allocation failed");
        exit(EXT_ERROR_THREAD_ARGS_ALLOCATE);
    }

    // start up workers
    int n_reqs_per_worker = args->n_requests / n_threads;
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

    // wait for workers to finish
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

    calc_print_stats(args, worker_args, n_threads, work_time);

    // can free that memory for requests now, if we needed it
    if (custom_request_flag()) {
        free(args->request.content);
    }
    free(workers);
    free(worker_args);
}

static void handle_send(int epoll_fd, socket_data_t *socket_data, request_t request);
// return 1 if this socket is done, else 0
static int handle_recv(int epoll_fd, socket_data_t *socket_data);

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
        socket_data_t *socket_data = &args->socket_datas[i];

        socket_data->fd = get_connected_socket(args->host);

        socket_data->current_request = 0;
        socket_data->current_response = 0;
        socket_data->n_requests = n_reqs_per_socket;

        // runtime dependent num of requsts per socket
        socket_data->timings = calloc(n_reqs_per_socket, sizeof(request_timing_t));
        if (socket_data->timings == NULL) {
            fprintf(stderr, "Timing data memory allocation failed");
            exit(EXT_ERROR_TIMING_DATA_ALLOCATE);
        }

        // and events to track
        struct epoll_event event = {0};
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR;
        event.data.fd = socket_data->fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_data->fd, &event) < 0) {
            perror("Epoll event for socket failed to be added");
            exit(EXT_ERROR_EPOLL_ADD_EVENT);
        }

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

        // handle all events
        for (int e = 0; e < n_events; e++) {
            int event_socket_fd = events[e].data.fd;

            switch (events[e].events) {
            case (EPOLLERR):
                perror("Epoll socket event failed, closing that socket");
                if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_socket_fd, NULL))
                    perror("Removing socket from epoll failed");

                close(event_socket_fd);
                n_sockets_connected--;
                break;
            case (EPOLLIN):
                for (int i = 0; i < args->n_concurrent; i++) {
                    if (args->socket_datas[i].fd == event_socket_fd) {
                        if (handle_recv(epoll_fd, &args->socket_datas[i]) == 1) {
                            n_sockets_connected--;
                        }
                    }
                }
                break;
            case (EPOLLOUT):
                for (int i = 0; i < args->n_concurrent; i++) {
                    if (args->socket_datas[i].fd == event_socket_fd) {
                        handle_send(epoll_fd, &args->socket_datas[i], args->request);
                        break;
                    }
                }
                break;
            default:
                fprintf(stderr, "Unkown Epoll event\n");
                break;
            }
        }
    }

    struct timespec end_batch_time;
    clock_gettime(CLOCK_MONOTONIC, &end_batch_time);
    args->batch_time = (end_batch_time.tv_sec - start_batch_time.tv_sec) +
                       (end_batch_time.tv_nsec - start_batch_time.tv_nsec) / 1e9;

    // shouldn't need to close any sockets... should close in event loop
    close(epoll_fd);
    return NULL;
}

void calc_print_stats(const args_t *args, const worker_args_t *worker_datas, int n_workers,
                      double work_time) {
    // a little space here, huh
    printf("\n\n");
    printf("Total Work Time (includes socket creation, etc.) : %.9f\n", work_time);

    int n_reqs_per_worker = args->n_requests / n_workers;
    int n_reqs_per_socket = n_reqs_per_worker / args->n_concurrent;

    // print out our results
    double avg_throughput_threads = 0.0;
    double avg_latency_threads = 0.0;

    for (int t = 0; t < n_workers; t++) {
        printf("Worker %d stats ----\n", t);

        double avg_latency_thread = 0.0;
        for (int s = 0; s < args->n_concurrent; s++) {
            printf("	Socket %d stats --\n", s);

            double avg_latency_socket = 0.0;
            for (int i = 0; i < n_reqs_per_socket; i++) {
                // calculate the latency of this request
                request_timing_t *timing = &worker_datas[t].socket_datas[s].timings[i];

                double latency = (timing->recv_time.tv_sec - timing->send_time.tv_sec) +
                                 (timing->recv_time.tv_nsec - timing->send_time.tv_nsec) / 1e9;
                avg_latency_socket += latency;
                avg_latency_thread += latency;
            }
            avg_latency_socket /= n_reqs_per_socket;

            printf("	  Average request latency : %0.15f seconds\n", avg_latency_socket);

            // we can free this socket's timing data now
            free(worker_datas[t].socket_datas[s].timings);
        }
        avg_latency_thread /= n_reqs_per_worker;

        double throughput_thread = n_reqs_per_worker / worker_datas[t].batch_time;

        printf("\n	Worker batch time : %d requests in %0.9f seconds\n", n_reqs_per_worker,
               worker_datas[t].batch_time);
        printf("	Worker average request latency : %0.9f seconds\n", avg_latency_thread);
        printf("	Worker request throughput : %0.9f requests/second\n", throughput_thread);

        avg_throughput_threads += throughput_thread;
        avg_latency_threads += avg_latency_thread;

        // we can free this threads socket datas now
        free(worker_datas[t].socket_datas);
    }

    avg_throughput_threads /= n_workers;
    avg_latency_threads /= n_workers;

    printf("\n\n");
    printf("Overall stats ----\n");
    printf("	Average thread throughput : %0.9f\n", avg_throughput_threads);
    printf("	Average thread latency : %0.9f\n", avg_latency_threads);
}

void handle_send(int epoll_fd, socket_data_t *socket_data, request_t request) {
    // done sending messages
    if (socket_data->current_request >= socket_data->n_requests) {
        // turn off tracking EPOLLOUT events for this socket
        struct epoll_event event = {0};
        event.events = EPOLLIN | EPOLLERR;
        event.data.fd = socket_data->fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, socket_data->fd, &event))
            perror("Removing EPOLLOUT event failed");
        return;
    }

    send_http_request(socket_data->fd, request);

    // record the start time of this request
    struct timespec *send_time = &socket_data->timings[socket_data->current_request].send_time;
    clock_gettime(CLOCK_MONOTONIC, send_time);
    socket_data->current_request++;
}

int handle_recv(int epoll_fd, socket_data_t *socket_data) {
    // have received all messages... close, could maybe move this to
    // a loop at the end to avoid syscall overhead for other sockets still
    // sending, but i lazy
    if (socket_data->current_response >= socket_data->n_requests) {
        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_data->fd, NULL))
            perror("Removing socket from epoll failed");

        close(socket_data->fd);
        return 1;
    }

    char response_buffer[4096] = {0};
    recv_http_response(socket_data->fd, response_buffer, 4096);

    // record the time, assuming we get all the responses in the order
    // we sent... sort of big assumption... oops
    struct timespec *end_time = &socket_data->timings[socket_data->current_response].recv_time;
    clock_gettime(CLOCK_MONOTONIC, end_time);
    socket_data->current_response++;
    return 0;
}
