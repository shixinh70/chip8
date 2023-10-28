CFLAGS=-std=c17 -Wall -Wextra -Werror `sdl2-config --cflags --libs`
all:
	gcc chip8.c -o chip8 $(CFLAGS) 
debug:
	gcc chip8.c -o chip8 $(CFLAGS) -DDEBUG

clean:
	rm chip8 *.o
