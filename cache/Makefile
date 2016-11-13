# Compiler and flags
CC=gcc
FLAGS=-Wall

# Source codes
SOURCES=cache.c

all: cache cache_stride_prefetcher

cache: ${SOURCES}
	${CC} $^ ${FLAGS} -o $@

cache_stride_prefetcher: ${SOURCES}
	${CC} $^ ${FLAGS} -DCACHE_PREFETCHER=stride_based_prefetcher -o $@

clean:
	rm -f cache cache_stride_prefetcher
