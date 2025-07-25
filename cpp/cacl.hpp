#ifndef _CACL_HPP
#define _CACL_HPP

#include <functional>
#include <iostream>
#include <string>

namespace cacl {

enum ReturnCode { C_OK, C_ERR, C_UNKNOWN };

enum TokenType {
  T_UNKNOWN,
  T_SEP,
  T_EOL,
  T_STRING,
  T_ESC,
  T_CMD,
  T_VAR,
};

#define C_TRACE_PARSER_BIT 0x1

#define C_TRACE_PARSER(x)                                                      \
  if (C_TRACE & C_TRACE_PARSER_BIT) {                                          \
    std::cout << "at: " << i << ' ' << x << std::endl;                         \
  }

#ifndef C_TRACE
#define C_TRACE C_TRACE_PARSER_BIT
#endif

struct Parser {
  Parser(const std::string &buffer_)
      : buffer(buffer_), result(""), i(0), start(0), end(0), token(T_UNKNOWN),
        insidequote(false) {}

  std::string buffer;
  std::string result;

  // String iterator
  size_t i;

  // Start and end of the specific thing being parsed in the string
  size_t start, end;

  // Type of token parser
  TokenType token;

  // Whether inside a quote or not
  bool insidequote;

  bool done() const { return i >= buffer.length(); }
  char current() const { return buffer.at(i); }
  TokenType tokenType() const { return token; }

  /*
   * Saves place within the parser
   */
  template <typename Func> ReturnCode save(Func fn) {
    start = i;
    ReturnCode r = fn();
    end = i;
    return r;
  }

  /**
   * Separator -- simply consumes non-newline whitespace
   */
  ReturnCode parseSep() {
    token = T_SEP;
    return save([this]() {
      while (!done() &&
             (current() == '\t' || current() == '\n' || current() == '\r')) {
        i += 1;
      }
      return C_OK;
    });
  }

  /**
   * EOL
   */
  ReturnCode parseEol() {
    token = T_EOL;
    return save([this]() {
      while (!done() &&
             (current() == ' ' || current() == '\t' || current() == '\n' ||
              current() == '\r' || current() == ';')) {
        i += 1;
      }
      return C_OK;
    });
  }

  /**
   * String parser; bulk of the actual parser
   * Begins by detecting whether this is a new word
   * (previous token was a separator or another string)
   * Handles starting brace beginning
   */
  ReturnCode parseString() {
    token = T_STRING;
    bool new_word = token == T_SEP || token == T_EOL || token == T_STRING;

    // Check whether we need to set inside quote and consume "
    if (new_word && current() == '"') {
      insidequote = true;
      i++;
    }

    return save([this]() {
      while (true) {
        if (done()) {
          token = T_ESC;
          return C_OK;
        }
        switch (current()) {
        case '"': {
          // If we're inside quote already this means it's time to escape
          if (insidequote) {
            token = T_ESC;
            insidequote = false;
            i += 1;
            return C_OK;
          }
        }
        }

        i += 1;
      }
      return C_OK;
    });
  }

  ReturnCode getToken() {
    while (!done()) {
      char c = current();
      C_TRACE_PARSER("char: " << c);
      switch (c) {
      case ' ':
      case '\t':
      case '\r':
        return parseSep();
      case '\n':
      case ';':
        return parseEol();
      case '"':
        return parseString();
      default:
        return parseString();
      }
    }
    std::cout << "reached end" << std::endl;
    token = T_EOL;
    return C_OK;
  }
};

struct Interp {
  void eval(const std::string &str) {}
}

} // namespace cacl

#endif