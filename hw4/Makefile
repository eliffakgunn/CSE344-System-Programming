all: program
program: program.o
	gcc -pthread program.c -lm -o program -lrt -lpthread -Wall -Wextra -pedantic
program.o: program.c
	gcc -c program.c
clean: 
	rm -rf *.o program  

