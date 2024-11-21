# Overview
Simple little http benchmarker, no libraries just sockets, pthreads, and epoll. Think this could majorly benefit from an arena style memory allocator, keep accesses to latency info more friendly, possible lifetime issues go away (valgrind checks out ok for now, but it would be simpler with shared lifetimes).

## Build
To build
``` bash
make
```

To build and run with defaults described below. Make sure the server is running.
``` bash
make bench
```

## Usage
Set up by default for .env.example port on local machine, 1000 requests divided amongst 4 workers, HTTP request is just a simple GET.

``` bash
./client {args}
```

args available
- -n {number of requests to send}
- -j {number of threads to spawn} will distribute requests evenly between them
- -c {number of concurrent sockets per thread} will distribute further the requests
- -r {custom http request} a string "" of your http request
- -s {server to benchmark of the form [ip:port]}
- -v verbose (prints (truncated) http response to console) no argument modifier
- -f {filepath} output is redirected to filepath... coming soon

## Output

Will return some stats, such as:

- Total time for batch per worker
- Avg latency per request per worker
- Throughput per worker
