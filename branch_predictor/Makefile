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
SOURCES=branch_predictor.c

all: not_taken_predictor two_bit_predictor two_level_predictor perceptron_predictor

not_taken_predictor: ${SOURCES}
	${CC} $^ ${FLAGS} -DBRANCH_PREDICTOR=not_taken_predictor -o $@

two_bit_predictor: ${SOURCES}
	${CC} $^ ${FLAGS} -DBRANCH_PREDICTOR=two_bit_predictor -o $@

two_level_predictor: ${SOURCES}
	${CC} $^ ${FLAGS} -DBRANCH_PREDICTOR=two_level_predictor_v2 -o $@

perceptron_predictor: ${SOURCES}
	${CC} $^ ${FLAGS} -DBRANCH_PREDICTOR=perceptron_predictor -o $@

clean:
	rm -f not_taken_predictor two_bit_predictor two_level_predictor perceptron_predictor
