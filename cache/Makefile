# Compiler and flags
CC=gcc
FLAGS=-Wall

# Source codes
SOURCES=cache.c

all: cache

cache: ${SOURCES}
	${CC} $^ ${FLAGS} -o $@

clean:
	rm -f cache
