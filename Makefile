CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

all: server client

server: src/server.c
	$(CC) $(CFLAGS) -O3 src/server.c -o agent_blend_server

client: src/client.c
	$(CC) $(CFLAGS) -O3 src/client.c -o agent_blend_cli

clean:
	rm -f agent_blend_server agent_blend_cli *.o

.PHONY: all server client clean
