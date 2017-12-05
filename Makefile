CC=gcc
CFLAGS=-c -g -Wall -Wextra
LFLAGS=-Wall

.PHONY: clean all

all: util.o proxy

proxy: proxy.c util.o
	$(CC) $(LFLAGS) $^ -o $@

util.o: util.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f proxy
	rm -f *.o

