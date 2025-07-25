.PHONY: leaks

picol: picol.c
	clang -g -o $@ $<

leaks:
	leaks --atExit -- ./picol empty.tcl
