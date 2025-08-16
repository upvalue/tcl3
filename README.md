# tcl3

This was an experiment in implementing a simple Tcl interpreter in C++, Zig and Rust. Just to get a sense of how they feel.

# Building

You can run the script `./build.sh` which will attempt to build all
implementations and report status -- it assumes the toolchain for each language
is accessible in the environment so if you don't have e.g. Zig installed then
that build will fail. It also assumes you have GNU make installed as `make`.

Zig was at 0.14.1, not tested on 0.15 and will likely fail. 

# Low hanging fruit

Some low hanging fruit if you wanted to improve:

- Use hash tables for commands at the least, maybe variables
- Cache integer parsing, make individual math commands
- Add some basic metaprogramming and list functionality
- Could reduce allocs further by keeping a buffer around for result
- Make the code more idiomatic for languages (e.g. use iterators for Parser)
- Fix ProcPrivdata -- it's intended to be user extensible but is just a little
  silly in Zig and Rust code

- A lot more tests, I bet there's a few bugs lurking

# Testing 

There is a hand-rolled testing framework in `test.py` that tests the parser and
interpreter, to make sure that behavior matches across implementations. 

> python test.py 

Will test picol itself.

> python test.py --impl ./cpp/repl

Will test the C++ implementation, etc.

Finally

> python test.py --update

Will update test results with the actual results.

This is a snapshot testing approach: the parser can output token information
(token type, body, start and end) as JSON lines to stderr, and the interpreter
can simply use the `puts` function to output some calculation to stdout. If the
output doesn't match the given `out` file or the output file doesn't exist the
test is considered a failure, and the implementation should be fixed before
updating the snapshots.

Note that picol "fails" a couple of the parser tests because I rewrote the
parser for the other implementations -- I don't think the behavior differences
are meaningful but let me know if there are any parser bugs.
