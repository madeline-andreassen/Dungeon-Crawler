heap: heap.o dungeon_generation.o
	gcc heap.o dungeon_generation.o -o heap -lncurses
heap.o: heap.c heap.h macros.h
	gcc -Wall -g heap.c -c
dungeon_generation.o: dungeon_generation.c heap.h
	gcc -Wall -g dungeon_generation.c -c 
	