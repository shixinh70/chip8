CFLAGS=-std=c17 -Wall -Wextra -Werror `sdl2-config --cflags --libs`
all:
	gcc chip8.c -o chip $(CFLAGS) 