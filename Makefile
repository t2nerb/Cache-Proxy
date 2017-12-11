CC=gcc
CFLAGS=-c -g -Wall -Wextra
LFLAGS=-Wall
LDFLAGS=-lssl -lcrypto

.PHONY: clean all

all: util.o proxy

proxy: proxy.c util.o
	$(CC) $(LFLAGS) $^ -o $@ $(LDFLAGS)

util.o: util.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f proxy
	rm -f *.o
	rm -f Cache/*
