build:
	gcc -Wall -Wextra -g -std=gnu99 ./src/*.c -o client

bench: build
	./client

clean:
	rm -rf ./client
