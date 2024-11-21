#include "args.h"
#include "benchmark.h"

int main(int argc, char **argv) {
    // sensible defaults
    args_t args = {
        .n_requests = 1000,
        .n_threads = 4,
        .n_concurrent = 10,
        .host = {.ip = "127.0.0.1", .port = 17299},
        .request = {.content = "GET / HTTP/1.1\r\nHost: 127.0.0.1:17299\r\n\r\n", .length = 41},
    };

    arg_parse(argc, argv, &args);

    bench_http_request(&args);

    return EXT_SUCESS;
}
