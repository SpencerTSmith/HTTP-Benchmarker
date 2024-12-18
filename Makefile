build:
	gcc -Wall -Wextra -g -std=gnu11 ./src/*.c -o client

clean:
	rm -rf ./client
