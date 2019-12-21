all: aufgabe2
# Which compiler
CC = gcc
# Where the include files are kept
INCLUDE = .
CFLAGS = -Wall -std=c99
aufgabe2: main.o utils.o threads.o
		$(CC) -o aufgabe2 main.o utils.o threads.o -pthread
main.o: main.c Header.h
		$(CC) -I$(INCLUDE) $(CFLAGS) -c main.c
utils.o: utils.c Header.h
		$(CC) -I$(INCLUDE) $(CFLAGS) -c utils.c
threads.o: threads.c Header.h
		$(CC) -I$(INCLUDE) $(CFLAGS) -c threads.c
install: aufgabe2

