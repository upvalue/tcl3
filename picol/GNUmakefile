.PHONY: leaks

CC := cc
CFLAGS := -O3 -g3

all: picol picol-original

picol: picol.c
	${CC} ${CFLAGS} -g -o $@ $<

picol-original: picol-original.c
	${CC} $(CFLAGS) -o $@ $<

picol-malloc-track: picol-original.c
	${CC} ${CFLAGS} $(shell pkg-config --libs jemalloc) -g -o $@ $< 

leaks:
	leaks --atExit -- ./picol empty.tcl
