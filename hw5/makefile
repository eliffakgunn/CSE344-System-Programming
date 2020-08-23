all: floristApp
floristApp: floristApp.o
	gcc -pthread floristApp.c -std=gnu99 -lm -o floristApp -lrt -lpthread -Wall -Wextra -pedantic
floristApp.o: floristApp.c
	gcc -c floristApp.c
clean: 
	rm -rf *.o floristApp  

