# Overview
Simple little http benchmarker, no libraries just sockets and pthreads. Soon will switch over to asynchronous sockets in combination with threads.

## Build
To build and run with defaults described below. Linux only.
```
make bench
```

## Usage
Set up by default for class project default port on local machine, no args gives default of 1000 requests divided amongst 4 workers 

- -n {number of requests to send}
- -j {number of threads to spawn} will distribute requests evenly between them
- -r {custom http request} coming soon
- -s {server to benchmark of the form [ip:port]} coming soon

## Output

Will return some stats, such as:

- Total time for batch per worker
- Avg latency per request per worker
- Throughput per worker
