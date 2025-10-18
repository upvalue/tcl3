CXX := clang++
CC := clang
CXXFLAGS := -std=c++20 -O3 -g3 -I../vendor

repl: repl.cpp tcl.cpp tcl.h linenoise.o
	$(CXX) $(CXXFLAGS) -o repl repl.cpp tcl.cpp linenoise.o

linenoise.o: ../vendor/linenoise.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f repl linenoise.o 
