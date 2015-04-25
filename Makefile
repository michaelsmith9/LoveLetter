CC = gcc
CFLAGS = -Wall -pedantic -std=gnu99
TARGETS = player hub

.DEFAULT: all
.PHONY: all debug clean

all: $(TARGETS)

player: player.c
	$(CC) $(CFLAGS) player.c -o player

hub: hub.c
	$(CC) $(CFLAGS) hub.c -o hub
