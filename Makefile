build:
	gcc -Wall -Wextra -g -std=gnu99 ./src/*.c -o client

bench: build
	./client

bench-exercises: build
	./client -r $'GET /levels HTTP/1.1\r\nHost: 127.0.0.1:17299\r\n\r\n'
clean:
	rm -rf ./client
