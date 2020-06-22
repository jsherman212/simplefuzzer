CC=clang
CFLAGS=-g -fsanitize=address

all: simplefuzz.c
	$(CC) $(CFLAGS) simplefuzz.c -o simplefuzz
