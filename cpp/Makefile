CXX := clang++
CC := clang
CXXFLAGS := -std=c++17

cacl: repl.cpp vendor/bestline/bestline.o | cacl.hpp
	$(CXX) -Ivendor/bestline -o repl $^

test: test.cpp cacl.hpp
	$(CXX) $(CXXFLAGS) -Ivendor/bestline -o test $<

vendor/bestline/bestline.o:
	$(CC) -O2 -c vendor/bestline/bestline.c -o vendor/bestline/bestline.o

clean:
	rm -f woof test vendor/bestline/bestline.o