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
