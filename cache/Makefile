#
#  Branch Predictors Comparisons
#
#  Copyright (C) 2016  Mateus Ravedutti Lucio Machado
#                      Rafael Ravedutti Lucio Machado
# 
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

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
