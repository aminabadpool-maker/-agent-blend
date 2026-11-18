CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -Iinclude

all: server client load_test

server: src/server.c
	$(CC) $(CFLAGS) -O3 src/server.c -o agent_blend_server

client: src/client.c
	$(CC) $(CFLAGS) -O3 src/client.c -o agent_blend_cli

load_test: tests/load_test.c
	$(CC) $(CFLAGS) -O3 tests/load_test.c -o agent_load_test

clean:
	rm -f agent_blend_server agent_blend_cli agent_load_test *.o

.PHONY: all server client load_test clean
