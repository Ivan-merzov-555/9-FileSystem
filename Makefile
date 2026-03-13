
CC = gcc
CFLAGS = -Wall -g
LIBS = -lncurses

all: task1 task2

task1: task1.c
	$(CC) $(CFLAGS) -o task1 task1.c

task2: task2.c
	$(CC) $(CFLAGS) -o task2 task2.c $(LIBS)

clean:
	rm -f task1 task2 output.txt

run1: task1
	./task1

run2: task2
	./task2

.PHONY: all clean run1 run2