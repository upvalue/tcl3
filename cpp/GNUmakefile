CXX := clang++
CC := clang
CXXFLAGS := -std=c++17 -O3

repl: repl.cpp vendor/bestline/bestline.o tcl.hpp
	$(CXX) $(CXXFLAGS) -Ivendor/bestline -o repl repl.cpp vendor/bestline/bestline.o

repl-malloc-track: repl.cpp vendor/bestline/bestline.o tcl.hpp
	$(CXX) $(shell pkg-config --libs jemalloc) -Ivendor/bestline -o $@ repl.cpp vendor/bestline/bestline.o

test: test.cpp tcl.hpp
	$(CXX) $(CXXFLAGS) -Ivendor/bestline -o test $<

vendor/bestline/bestline.o:
	$(CC) -O2 -c vendor/bestline/bestline.c -o vendor/bestline/bestline.o

clean:
	rm -f woof test vendor/bestline/bestline.o
