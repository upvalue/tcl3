CXX := clang++
CC := clang
CXXFLAGS := -std=c++20 -O3 -g3 -I../vendor

repl: repl.cpp tcl.hpp linenoise.o
	$(CXX) $(CXXFLAGS) -o repl repl.cpp linenoise.o

linenoise.o: ../vendor/linenoise.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f repl 
