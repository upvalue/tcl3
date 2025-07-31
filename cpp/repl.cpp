#include "tcl.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace tcl;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  bool trace_parser = false;

  if (getenv("PARSER_STDERR")) {
    trace_parser = true;
  }

  Interp i;
  i.trace_parser = trace_parser;
  i.register_core_commands();

  i.register_command("testret", [](Interp &i, std::vector<std::string> &argv,
                                   Privdata *privdata) {
    std::cout << "it got called!" << std::endl;
    return S_OK;
  });

  // Open and read the file
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

  std::cout << "eval_invokes: " << i.eval_invokes << std::endl;
  // Print result if any
  if (!i.result.empty()) {
    // std::cout << i.result << std::endl;
  }

  return 0;
}