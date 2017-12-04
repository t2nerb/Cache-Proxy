CC=gcc
CFLAGS=-Wall

.PHONY: clean all

all: proxy

proxy: proxy.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f proxy
	rm -f *.o

