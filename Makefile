build:
	gcc -Wall -Wextra -g *.c -o client

run: build
	./client

clean:
	rm -rf ./client
