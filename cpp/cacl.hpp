#ifndef _CACL_HPP
#define _CACL_HPP

#include <functional>
#include <iostream>
#include <string>

namespace cacl {

enum ReturnCode { C_OK = 0, C_ERR = 1, C_UNKNOWN = 2 };

enum TokenType {
  TK_ESC = 0,
  TK_STR = 1,
  TK_CMD = 2,
  TK_VAR = 3,
  TK_SEP = 4,
  TK_EOL = 5,
  TK_EOF = 6,
  TK_UNKNOWN = 7,
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
      : buffer(buffer_), result(""), i(0), start(0), end(0), token(TK_UNKNOWN),
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
    token = TK_SEP;
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
    token = TK_EOL;
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
    token = TK_STR;
    bool new_word = token == TK_SEP || token == TK_EOL || token == TK_STR;

    // Check whether we need to set inside quote and consume "
    if (new_word && current() == '"') {
      insidequote = true;
      i++;
    }

    return save([this]() {
      while (true) {
        if (done()) {
          token = TK_ESC;
          return C_OK;
        }
        switch (current()) {
        case '"': {
          // If we're inside quote already this means it's time to escape
          if (insidequote) {
            token = TK_ESC;
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

  std::string tokenBody() { return buffer.substr(start, end - start); }

  ReturnCode nextToken() {
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
    token = TK_EOL;
    return C_OK;
  }
};

struct Interp;

typedef ReturnCode(cmd_func_t)(Interp *i, std::vector<std::string> &argv,
                               void *privdata);

struct Cmd {
  Cmd(const std::string &name_, cmd_func_t *func_, void *privdata_, Cmd *next_)
      : name(name_), func(func_), privdata(privdata_), next(next_) {}
  ~Cmd() {}

  std::string name;
  cmd_func_t *func;
  void *privdata;
  Cmd *next;
};

struct Interp {
  Cmd *commands;
  std::string result;
  std::string errbuf;

  Interp() : commands(0) {}

  ReturnCode eval(const std::string &str) {
    result = "";
    Parser p(str);
    ReturnCode ret = C_OK;
    std::vector<std::string> argv;
    while (1) {
      TokenType prevtype = p.token;
      ret = p.nextToken();
      std::cout << "token: " << p.token << std::endl;
      if (ret != C_OK) {
        return ret;
      }

      if (p.token == TK_EOF) {
        break;
      }

      if (p.token == TK_CMD) {
        std::cout << "got command!" << std::endl;
      } else if (p.token == TK_ESC) {
        continue;
      }

      if (p.token == TK_EOL) {
        Cmd *c;
        prevtype = p.token;

        if (argv.size()) {
          if ((c = getCommand(argv[0])) == nullptr) {
            errbuf = "command not found";
            return C_ERR;
          }

          c->func(this, argv, c->privdata);
        }

        std::cout << "EOL. Going to evaluate " << p.buffer << std::endl;
      }

      argv.clear();

      if (prevtype == TK_SEP || prevtype == TK_EOL) {
        argv.push_back(p.tokenBody());
      }
    }
    return C_OK;
  }

  Cmd *getCommand(const std::string &name) const {
    for (Cmd *c = commands; c != nullptr; c = c->next) {
      if (c->name.compare(name) == 0) {
        return c;
      }
    }
    return nullptr;
  }

  ReturnCode register_command(const std::string &name, cmd_func_t fn,
                              void *privdata = nullptr) {
    Cmd *chk = getCommand(name);
    if (chk != nullptr) {
      errbuf = "command already defied";
      return C_ERR;
    }
    Cmd *cmd = new Cmd(name, fn, privdata, commands);
    commands = cmd;
    return C_OK;
  }

  void register_core_commands() {}
};

} // namespace cacl

#endif