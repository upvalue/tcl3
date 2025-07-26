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

inline std::ostream &operator<<(std::ostream &os, TokenType t) {

  switch (t) {
  case TK_ESC:
    os << "TK_ESC";
    break;
  case TK_STR:
    os << "TK_STR";
    break;
  case TK_CMD:
    os << "TK_CMD";
    break;
  case TK_VAR:
    os << "TK_VAR";
    break;
  case TK_SEP:
    os << "TK_SEP";
    break;
  case TK_EOL:
    os << "TK_EOL";
    break;
  case TK_EOF:
    os << "TK_EOF";
    break;
  case TK_UNKNOWN:
    os << "TK_UNKNOWN";
    break;
  }
  return os;
}

#define C_TRACE_PARSER_BIT 0x1
#define C_TRACE_EVAL_BIT 0x2

#define C_TRACE_PARSER(x)                                                      \
  if (C_TRACE & C_TRACE_PARSER_BIT) {                                          \
    std::cout << "at: " << i << ' ' << x << std::endl;                         \
  }

#define C_TRACE_EVAL(x)                                                        \
  if (C_TRACE & C_TRACE_EVAL_BIT) {                                            \
    std::cout << x << std::endl;                                               \
  }

#ifndef C_TRACE
#define C_TRACE C_TRACE_PARSER_BIT | C_TRACE_EVAL_BIT
#endif

// Typedef std::string to make it easier to experiment with drop in replacements
typedef std::string string;

struct Parser {
  Parser(const string &buffer_)
      : buffer(buffer_), result(""), i(0), start(0), end(0), token(TK_EOL),
        insidequote(false) {}

  string buffer;
  string result;

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
      while (!done() && (current() == ' ' || current() == '\t' ||
                         current() == '\n' || current() == '\r')) {
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
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case ';': {
          if (!insidequote) {
            token = TK_ESC;
            return C_OK;
          }
          break;
        }
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

  string tokenBody() { return buffer.substr(start, end - start); }

  ReturnCode _nextToken() {
    while (!done()) {
      char c = current();
      switch (c) {
      case ' ':
      case '\t':
      case '\r':
        if (insidequote) {
          return parseString();
        }
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

    // If we've reached the end of the buffer,
    // if there's a non-EOL non-EOF token present, we emit an EOL
    // to cause the interpreter to actually evaluate the string

    // If EOL, we've been called again and emit an EOF to cease execution
    if (token != TK_EOL && token != TK_EOF) {
      token = TK_EOL;
    } else {
      token = TK_EOF;
    }

    return C_OK;
  }

  ReturnCode nextToken() {
    ReturnCode ret = _nextToken();
    C_TRACE_PARSER("token type: " << token << " token body: '" << tokenBody()
                                  << "'");
    return ret;
  }
};

struct Interp;

typedef ReturnCode(cmd_func_t)(Interp *i, std::vector<string> &argv,
                               void *privdata);

struct Cmd {
  Cmd(const string &name_, cmd_func_t *func_, void *privdata_, Cmd *next_)
      : name(name_), func(func_), privdata(privdata_), next(next_) {}
  ~Cmd() {}

  string name;
  cmd_func_t *func;
  void *privdata;
  Cmd *next;
};

struct Interp {
  Cmd *commands;
  string result;
  string errbuf;

  Interp() : commands(0) {}

  ReturnCode eval(const string &str) {
    result = "";
    Parser p(str);
    ReturnCode ret = C_OK;

    // Tracks command and argument
    std::vector<string> argv;

    while (1) {
      // Previous token type -- note that the parser default value (TK_EOL) is
      // load bearing
      TokenType prevtype = p.token;

      ret = p.nextToken();
      // Exit if there's a parser error
      if (ret != C_OK) {
        return ret;
      }

      // Exit if we're at EOF
      if (p.token == TK_EOF) {
        break;
      }

      if (p.token == TK_CMD) {
        std::cout << "got command!" << std::endl;
      } else if (p.token == TK_ESC) {
        // No escape handling
      } else if (p.token == TK_SEP) {
        prevtype = p.token;
        continue;
      }

      // Once we hit EOL, we should have a command to evaluate
      if (p.token == TK_EOL) {
        Cmd *c;
        prevtype = p.token;

        // Look up the command; if we find one,
        // call it with the value otherwise
        // return an error
        if (argv.size()) {
          if ((c = getCommand(argv[0])) == nullptr) {
            errbuf.clear();
            errbuf += "command not found: '";
            errbuf += argv[0];
            errbuf += "'";
            return C_ERR;
          }

          c->func(this, argv, c->privdata);
        }

        // Clear arg vector for the next run and continue
        argv.clear();
        continue;
      }

      // If last token was a separator or EOL, push the final token body
      // to the argument vector and continue
      if (prevtype == TK_SEP || prevtype == TK_EOL) {
        argv.push_back(p.tokenBody());
      } else {
        std::cout << "interpolation woah" << std::endl;
      }
    }
    return C_OK;
  }

  Cmd *getCommand(const string &name) const {
    for (Cmd *c = commands; c != nullptr; c = c->next) {
      if (c->name.compare(name) == 0) {
        return c;
      }
    }
    return nullptr;
  }

  ReturnCode register_command(const string &name, cmd_func_t fn,
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

  void register_core_commands() {
    register_command("puts",
                     [](Interp *i, std::vector<string> &argv, void *privdata) {
                       if (argv.size() != 2) {
                         return C_ERR;
                       }
                       std::cout << argv[1] << std::endl;
                       return C_OK;
                     });
  }
};

} // namespace cacl

#undef C_TRACE_PARSER
#undef C_TRACE_EVAL

#endif