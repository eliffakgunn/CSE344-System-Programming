all:
	gcc -std=gnu99 server.c -pthread -lrt -lm -o  server -Wall -Wextra -pedantic
	gcc -std=gnu99 client.c -pthread -lrt -lm -o  client -Wall -Wextra -pedantic

clean:
	rm -rf *.o server client

