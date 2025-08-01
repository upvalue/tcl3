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

struct Parser {
  Parser(const std::string_view &body_) : body(body_) {}

  const std::string_view body;
  size_t cursor = 0;
  size_t begin = 0, end = 0;

  // If inside a potentially multiline thing
  bool in_string = false;
  bool in_brace = false;
  bool in_quote = false;
  bool in_command = false;

  size_t brace_level = 0;
  Token token = TK_ESC;
  char terminating_char = 0;

  bool done() { return cursor >= body.size(); }
  char peek() { return body[cursor]; }
  char getc() { return body[cursor++]; }
  void back() { cursor--; }

  std::string_view token_body() { return body.substr(begin, end - begin); }

  /**
   * Consumes all whitespace until EOF or non-whitespace character
   */
  void consume_whitespace() {
    while (!done()) {
      char c = peek();
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
        getc();
      } else {
        break;
      }
    }
  }

  void recurse(Parser &sub, char terminating_char) {
    sub.terminating_char = terminating_char;
    while (true) {
      Token tk = sub.next_token();
      if (tk == TK_EOF) {
        break;
      }
    }
    cursor = cursor + sub.cursor;
  }

  Token next_token() {
    size_t adj = 0;
  begin:
    if (done()) {
      begin = end = cursor;
      return TK_EOF;
    }
    token = TK_ESC;
    begin = cursor;
    while (!done()) {
      char c = getc();
      if (terminating_char && c == terminating_char) {
        return TK_EOF;
      }
      switch (c) {
      case '{': {
        if (in_quote || in_string)
          continue;
        if (!in_brace) {
          // Ignore opening brace
          begin++;
          token = TK_STR;
          in_brace = true;
        }
        // Ignore brace
        brace_level++;
        break;
      }
      case '}': {
        if (in_quote || in_string)
          continue;
        if (brace_level > 0) {
          brace_level--;
          if (brace_level == 0) {
            in_brace = false;
            adj = 1; // skip }
            goto finish;
          }
          break;
        }
      }
      case '[': {
        if (in_quote || in_string || in_brace)
          continue;
        begin++;
        Parser sub(body.substr(cursor));
        recurse(sub, ']');
        adj = 1;
        token = TK_CMD;
        goto finish;
      }
      case '$': {
        if (in_string || in_brace)
          continue;
        begin++;
        token = TK_VAR;
        // Variables are not actually a string but we treat them as such
        // to give them the same lexical behavior
        in_string = true;
        break;
      }
      // Potentially a comment
      case '#': {
        if (in_string || in_quote)
          continue;
        // Consume until newline
        while (!done()) {
          if (getc() == '\n')
            break;
        }
        goto begin;
      }
      case '"': {
        if (in_quote) {
          in_quote = false;
          adj = 1;
          goto finish;
        }
        in_quote = true;
        begin++;
        continue;
      }
      case ' ':
      case '\n':
      case '\r':
      case '\t':
        // If we're in a multiline token or quote, we don't want to break out of
        // the loop, this becomes part of the token because it may be
        // significant whitespace
        if (in_brace || in_quote) {
          continue;
        }
        // If we're in a string, this terminates the string
        // We back up one in order to then tokenize the whole separator
        // (as it might be a significant portion of some other thing)
        if (in_string) {
          back();
          in_string = false;
          goto finish;
        }
        token = c == ' ' ? TK_SEP : TK_EOL;
        // Eagerly consume all further whitespace
        consume_whitespace();
        goto finish;
      default: {
        if (!in_quote && !in_brace) {
          in_string = true;
        }
      }
      }
    }
  finish:
    end = cursor - adj;
    return token;
  }
};

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
  Parser parser(content);
  Token tk;
  while (true) {
    tk = parser.next_token();
    // {"type": "TK_ESC", "begin": 19, "end": 19, "body": ""}
    // output jsonl without newlines
    std::cout << "{" << "\"type\": \"" << tk << "\","
              << " \"begin\": " << parser.begin << ","
              << " \"end\": " << parser.end << "," << "  \"body\": \""
              << escape_string(parser.token_body()) << "\"" << "}" << std::endl;
    if (tk == TK_EOF)
      break;
  }

  return 0;
}