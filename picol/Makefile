.PHONY: leaks

CC := cc

all: picol picol-parser picol-original

picol: picol.c
	${CC} -DPICOL_MAIN -g -o $@ $<

picol-parser: picol.c
	${CC} -DPICOL_MAIN -DPICOL_TRACE_PARSER -g -o $@ $<

picol-original: picol-original.c
	${CC} -o $@ $<

leaks:
	leaks --atExit -- ./picol empty.tcl
