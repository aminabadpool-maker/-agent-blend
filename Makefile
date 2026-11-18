CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

all: server

server: src/server.c
	$(CC) $(CFLAGS) -O3 src/server.c -o agent_blend_server

clean:
	rm -f agent_blend_server *.o

.PHONY: all server clean
