#include "tcl.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

#include "linenoise.h"

using namespace tcl;

int main(int argc, char *argv[]) {
  bool trace_parser = false;

  if (getenv("PARSER_STDERR")) {
    trace_parser = true;
  }

  Interp i;
  i.trace_parser = trace_parser;
  i.register_core_commands();

  // Open and read the file
  if (argc == 2) {

    std::ifstream file(argv[1]);
    if (!file.is_open()) {
      std::cerr << "Error: Cannot open file '" << argv[1] << "'" << std::endl;
      return 1;
    }

    // Read entire file content
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Evaluate the file content
    Status s = i.eval(content);
    if (s != S_OK) {
      std::cerr << "Error evaluating file: " << i.result << std::endl;
      return 1;
    }
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

      Status s = i.eval(ln);
      if (s != S_OK) {
        std::cerr << "Error evaluating line: " << i.result << std::endl;
      }
    }
  }

  return 0;
}