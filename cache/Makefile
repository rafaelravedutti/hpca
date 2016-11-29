# Compiler and flags
CC=gcc
FLAGS=-Wall

# Source codes
SOURCES=cache.c

all: cache cache_stride_prefetcher variable_length_delta_prefetcher

cache: ${SOURCES}
	${CC} $^ ${FLAGS} -DCACHE_PREFETCHER=no_prefetcher -o $@

cache_stride_prefetcher: ${SOURCES}
	${CC} $^ ${FLAGS} -DCACHE_PREFETCHER=stride_based_prefetcher -o $@

variable_length_delta_prefetcher: ${SOURCES}
	${CC} $^ ${FLAGS} -DCACHE_PREFETCHER=variable_length_delta_prefetcher -o $@

clean:
	rm -f cache cache_stride_prefetcher variable_length_delta_prefetcher
