# Overview
Simple little http benchmarker, no libraries just sockets and pthreads. Soon will switch over to asynchronous socket solution in combination with threads.

## Usage
Set up by default for class project, no args

- -n {number of requests to send}
- -j {number of threads to spawn} will distribute requests evenly between them
- -r {custom http request} coming soon
- -s {server to benchmark of the form [ip:port]} coming soon
