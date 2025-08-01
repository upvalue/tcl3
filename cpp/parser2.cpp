#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

using std::string;
using std::string_view;

enum Token {
  TK_ESC,
  TK_STR,
  TK_CMD,
  TK_VAR,
  TK_SEP,
  TK_EOL,
  TK_EOF,
};

/*
 * Print a string but escape whitespace
 */
struct escape_string {
  escape_string(const string_view &s_) : s(s_) {}
  const string_view s;
};

inline std::ostream &operator<<(std::ostream &os, const escape_string &e) {
  for (size_t i = 0; i < e.s.length(); i++) {
    if (e.s[i] == '\n') {
      os << "\\n";
    } else if (e.s[i] == '\r') {
      os << "\\r";
    } else if (e.s[i] == '\t') {
      os << "\\t";
    } else {
      os << e.s[i];
    }
  }
  return os;
}

inline std::ostream &operator<<(std::ostream &os, Token tk) {
  switch (tk) {
  case TK_ESC:
    return os << "TK_ESC";
  case TK_STR:
    return os << "TK_STR";
  case TK_CMD:
    return os << "TK_CMD";
  case TK_VAR:
    return os << "TK_VAR";
  case TK_SEP:
    return os << "TK_SEP";
  case TK_EOL:
    return os << "TK_EOL";
  case TK_EOF:
    return os << "TK_EOF";
  }
  return os << "TK_UNKNOWN";
}
int main(int argc, const char **argv) {
  // Read file as argument, parse and output
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <file>" << std::endl;
    return 1;
  }

  std::ifstream file(argv[1]);
  if (!file.is_open()) {
    std::cerr << "Failed to open file: " << argv[1] << std::endl;
    return 1;
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  Parser2 parser(content);
  Token tk;
  while (true) {
    tk = parser.next_token();
    // {"type": "TK_ESC", "begin": 19, "end": 19, "body": ""}
    // output jsonl without newlines
    std::cerr << "{" << "\"type\": \"" << tk << "\","
              << " \"begin\": " << parser.begin << ","
              << " \"end\": " << parser.end << "," << " \"body\": \""
              << escape_string(parser.token_body()) << "\"" << "}" << std::endl;
    if (tk == TK_EOF)
      break;
  }

  return 0;
}