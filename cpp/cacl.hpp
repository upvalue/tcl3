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
      : buffer(buffer_), i(0), start(0), end(0), token(T_UNKNOWN) {}

  std::string buffer;

  // String iterator
  size_t i;

  // Start and end of the specific thing being parsed in the string
  size_t start, end;

  TokenType token;

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

  ReturnCode getToken() {
    while (!done()) {
      char c = current();
      C_TRACE_PARSER("char: " << c);
      switch (c) {
      case ' ':
      case '\t':
      case '\r':
        return parseSep();
      default:
        return C_UNKNOWN;
      }
    }
    return C_UNKNOWN;
  }
};

} // namespace cacl

#endif