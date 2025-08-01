.PHONY: leaks

CC := cc
CFLAGS := -O3 -g3

all: picol picol-original

picol: picol.c
	${CC} ${CFLAGS} -g -o $@ $<

picol-original: picol-original.c
	${CC} $(CFLAGS) -o $@ $<

clean:
	rm -f picol picol-original picol-malloc-track

leaks:
	leaks --atExit -- ./picol empty.tcl
