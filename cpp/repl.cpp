
#include "cacl.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace cacl;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 1;
  }

  Interp i;
  i.register_core_commands();

  i.register_command(
      "testret", [](Interp &i, std::vector<std::string> &argv, void *privdata) {
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

  Var *x = i.get_var("x");

  // Print result if any
  if (!i.result.empty()) {
    std::cout << i.result << std::endl;
  }

  return 0;
}