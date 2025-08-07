#include "tcl.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "argh.h"
#include "linenoise.h"

using namespace tcl;

std::set<std::string> allowed_flags = {"t", "trace-parser", "p", "parser-only",
                                       "h", "help"};

void exec(Interp &i, const std::string &content, bool eval) {
  if (eval) {
    Status s = i.eval(content);
    if (s != S_OK) {
      std::cerr << "Error evaluating file: " << i.result << std::endl;
      return;
    }
  } else {
    Parser p(content);
    p.trace_parser = i.trace_parser;
    while (true) {
      Token t = p.next_token();
      if (t == TK_EOF) {
        break;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  argh::parser cmdl(argv);
  bool trace_parser = false;
  bool parser_only = false;

  for (const auto &flag : cmdl.flags()) {
    if (allowed_flags.find(flag) == allowed_flags.end()) {
      std::cerr << "Unknown flag: " << flag << std::endl;
      return 1;
    }
  }

  if (cmdl[{"-h", "--help"}]) {
    std::cout << "Usage: repl [options] [file]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  -t, --trace-parser   Enable parser tracing" << std::endl;
    std::cout << "  -p, --parser-only    Only parse the input, don't execute"
              << std::endl;
    std::cout << "  -h, --help    Show this help message" << std::endl;
    std::cout << "If no file is given, the REPL will start." << std::endl;
    return 0;
  }

  if (cmdl[{"-t", "--trace-parser"}]) {
    trace_parser = true;
  }

  if (cmdl[{"-p", "--parser-only"}]) {
    parser_only = true;
  }

  Interp i;
  i.trace_parser = trace_parser;
  i.register_core_commands();

  if (cmdl(1)) {
    std::ifstream file(cmdl[1]);
    if (!file.is_open()) {
      std::cerr << "Error: Cannot open file '" << cmdl[0] << "'" << std::endl;
      return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    exec(i, content, !parser_only);
  } else {
    while (true) {
      char *line = linenoise("tcl> ");
      if (!line) {
        break;
      }
      std::string ln(line);
      free(line);
      if (ln.empty()) {
        break;
      }

      exec(i, ln, !parser_only);
    }
  }
}