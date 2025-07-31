.PHONY: leaks

CC := cc
CFLAGS := -O3

all: picol picol-parser picol-original

picol: picol.c
	${CC} -DPICOL_MAIN -g -o $@ $<

picol-parser: picol.c
	${CC} -DPICOL_MAIN -DPICOL_TRACE_PARSER -g -o $@ $<

picol-original: picol-original.c
	${CC} $(CFLAGS) -o $@ $<

picol-malloc-track: picol-original.c
	${CC} ${CFLAGS} $(shell pkg-config --libs jemalloc) -g -o $@ $< 

leaks:
	leaks --atExit -- ./picol empty.tcl
