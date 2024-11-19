#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    EXT_SUCESS,
    EXT_ERR_SOCK_CREATE,
    EXT_ERR_SERVER_ADDRESS,
    EXT_ERR_CONNECTION,
    EXT_ERR_REQUEST_FAIL,
    NUM_EXT_CODE,
} exit_code_e;

typedef struct {
    char ip[256];
    int port;
} host_t;

typedef struct {
    char content[512];
    int length;
} request_t;

void bench_http_request(const host_t *host, const request_t *request, int n_requests) {
    // get socket
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        exit(EXT_ERR_SOCK_CREATE);
    }

    // define socket address
    struct sockaddr_in server_address = {
        .sin_family = AF_INET,
        .sin_port = htons(host->port), // conver to how the network defines port #
    };

    // convert to binary format of ip address
    if (inet_pton(AF_INET, host->ip, &server_address.sin_addr) <= 0) {
        perror("Conversion of server address failed");
        exit(EXT_ERR_SERVER_ADDRESS);
    }

    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address))) {
        perror("Connection failed");
        close(socket_fd);
        exit(EXT_ERR_CONNECTION);
    }

    if (send(socket_fd, request, request->length, 0) < 0) {
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
        printf("Response received (truncated): %.100s\n", response);
    }

    close(socket_fd);
}

int main() {
    host_t host = {
        .ip = "93.184.216.34",
        .port = 80,
    };

    request_t request = {
        .content = "GET / HTTP/1.1\r\n"
                   "Host: %s:%d\r\n"
                   "Connection: close\r\n\r\n",
    };
    request.length = strlen(request.content);

    bench_http_request(&host, &request, 1);

    return EXT_SUCESS;
}
