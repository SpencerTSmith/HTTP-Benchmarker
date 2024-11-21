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
    printf("Total Work Time (includes socket creation, etc.) : %.9f\n", work_time);

    int n_reqs_per_socket = n_reqs_per_worker / args->n_concurrent;

    // print out our results
    for (int t = 0; t < n_threads; t++) {
        printf("Worker %d stats ----\n", t);

        double avg_latency_thread = 0.0;
        for (int s = 0; s < args->n_concurrent; s++) {
            printf("	Socket %d stats --\n", s);

            double avg_latency_socket = 0.0f;
            for (int i = 0; i < n_reqs_per_socket; i++) {
                avg_latency_socket += worker_args[t].socket_datas[s].timings[i].latency;
            }
            avg_latency_socket /= n_reqs_per_socket;

            printf("	  Average request latency : %0.15f seconds\n", avg_latency_socket);

            avg_latency_thread += avg_latency_socket;

            // we can free that sockets timing data now
            free(worker_args[t].socket_datas[s].timings);
        }
        avg_latency_thread /= args->n_concurrent;

        double throughput_thread = n_reqs_per_worker / worker_args[t].batch_time;

        printf("\n	Request batch time : %d requests in %0.9f seconds\n", n_reqs_per_worker,
               worker_args[t].batch_time);
        printf("	Average request latency : %0.9f seconds\n", avg_latency_thread);
        printf("	Request throughput : %0.9f requests/second\n", throughput_thread);

        // we can free that this threads socket data now
        free(worker_args[t].socket_datas);
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
        args->socket_datas[i].current_response = 0;
        args->socket_datas[i].n_requests = n_reqs_per_socket;

        // runtime dependent num of requsts per socket
        args->socket_datas[i].timings = calloc(n_reqs_per_socket, sizeof(request_timing_t));
        if (args->socket_datas[i].timings == NULL) {
            fprintf(stderr, "Timing data memory allocation failed");
            exit(EXT_ERROR_TIMING_DATA_ALLOCATE);
        }

        // and an event to track every socket
        struct epoll_event event = {0};
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR;
        event.data.fd = args->socket_datas[i].fd;

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, args->socket_datas[i].fd, &event) < 0) {
            perror("Epoll event for socket failed to be added");
            exit(EXT_ERROR_EPOLL_ADD_EVENT);
        }

        // initial to get the event loop started
        send_http_request(args->request, &args->socket_datas[i]);
        args->socket_datas[i].current_request++;
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
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_socket_fd, NULL);
                close(event_socket_fd);
                n_sockets_connected--;

                // we have received a response
            } else if (events[e].events & EPOLLIN) {

                // find the socket we got an event for...
                // TODO(spencer): this might benefit from hashing at larger # of sockets
                for (int i = 0; i < args->n_concurrent; i++) {

                    if (args->socket_datas[i].fd == event_socket_fd) {
                        // this will also calc the timing
                        recv_http_response(&args->socket_datas[i]);
                        args->socket_datas[i].current_response++;

                        // have received all messages... close
                        if (args->socket_datas[i].current_response >=
                            args->socket_datas[i].n_requests) {
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, event_socket_fd, NULL);
                            close(event_socket_fd);
                            n_sockets_connected--;
                            break;
                        }

                        break;
                    }
                }

                // we have the ability to send a message
            } else if (events[e].events & EPOLLOUT) {

                // find the socket we got an event for...
                // TODO(spencer): this might benefit from hashing
                for (int i = 0; i < args->n_concurrent; i++) {

                    // we have this event's socket
                    if (args->socket_datas[i].fd == event_socket_fd) {

                        // done sending messages
                        if (args->socket_datas[i].current_request >=
                            args->socket_datas[i].n_requests) {
                            break;
                        }

                        // this will also calc the timing
                        send_http_request(args->request, &args->socket_datas[i]);

                        // I think I like this decrement here (not in the send_http func) so
                        // it's more obvious what its doing
                        args->socket_datas[i].current_request++;
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
