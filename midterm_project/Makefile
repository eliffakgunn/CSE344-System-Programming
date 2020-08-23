all: program
program: program.o
	gcc -pthread program.o -lm -o program -lrt
program.o: program.c
	gcc -c program.c
clean: 
	rm -rf *.o program  

