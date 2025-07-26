.PHONY: leaks

picol: picol.c
	clang -DPICOL_MAIN -DPICOL_TRACE_PARSER -g -o $@ $<

leaks:
	leaks --atExit -- ./picol empty.tcl
