# Overview
Simple little http benchmarker, no libraries just sockets and pthreads. Soon will switch over to asynchronous socket stuff in combination with threads. Also want to add feature where you can benchmark any arbitrary request.

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
- -r {custom http request} coming soon
- -s {server to benchmark of the form [ip:port]}
- -v verbose (prints(truncated) http response to console) no argument modifier

## Output

Will return some stats, such as:

- Total time for batch per worker
- Avg latency per request per worker
- Throughput per worker
