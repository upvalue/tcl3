#ifndef _CACL_HPP
#define _CACL_HPP

#include <cstring>
#include <functional>
#include <iostream>
#include <string>

// TODO: Variables
// TODO: More error handling

namespace cacl {

enum Status { S_OK = 0, S_ERR = 1, S_UNKNOWN = 2 };

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
  TokenType token_type() const { return token; }

  /**
   * Saves token place at the start of token
   * and the end of the function that's generating it
   */
  struct Save {
    Save(Parser *p_) : p(p_) { p->start = p->i; }
    ~Save() { p->end = p->i; }
    Parser *p;
  };

  /**
   * Separator -- triggered on any non-newline whitespace or a
   * semicolon and consumes all whitespace until next char or EOF
   */
  Status parse_sep() {
    token = TK_SEP;
    Save save(this);
    while (!done() && (current() == ' ' || current() == '\t' ||
                       current() == '\n' || current() == '\r')) {
      i += 1;
    }
    return S_OK;
  }

  /**
   * End of line handlers
   */
  Status parse_eol() {
    token = TK_EOL;
    Save save(this);
    // Discard any whitespace or separators present at end of line
    while (!done() &&
           (current() == ' ' || current() == '\t' || current() == '\n' ||
            current() == '\r' || current() == ';')) {
      i += 1;
    }
    return S_OK;
  }

  /**
   * Variable, parses $alpha1234_5 type strings
   */
  Status parse_var() {
    // Skip $
    i++;
    Save save(this);
    while (!done()) {
      if (isalnum(current()) || current() == '_') {
        i++;
      } else {
        break;
      }
    }

    // Treat standalone $ as a string with only $ as its contents
    if (start == i) {
      token = TK_STR;
    } else {
      token = TK_VAR;
    }

    return S_OK;
  }

  /**
   * String parser; bulk of the actual parser
   * Begins by detecting whether this is a new word
   * (previous token was a separator or another string)
   * Handles starting brace beginning
   */
  Status parse_string() {
    token = TK_STR;
    bool new_word = token == TK_SEP || token == TK_EOL || token == TK_STR;

    // Check whether we need to set inside quote and consume "
    if (new_word && current() == '"') {
      insidequote = true;
      i++;
    }

    Save save(this);
    while (true) {
      if (done()) {
        token = TK_ESC;
        return S_OK;
      }
      switch (current()) {
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case ';': {
        if (!insidequote) {
          token = TK_ESC;
          return S_OK;
        }
        break;
      }
      case '$':
        return parse_var();
      case '"': {
        // If we're inside quote already this means it's time to escape
        if (insidequote) {
          token = TK_ESC;
          insidequote = false;
          i += 1;
          return S_OK;
        }
      }
      }

      i += 1;
    }
    return S_OK;
  }

  string token_body() { return buffer.substr(start, end - start); }

  Status _next_token() {
    while (!done()) {
      char c = current();
      switch (c) {
      case ' ':
      case '\t':
      case '\r':
        if (insidequote) {
          return parse_string();
        }
        return parse_sep();
      case '\n':
      case ';':
        return parse_eol();
      case '"':
        return parse_string();
      default:
        return parse_string();
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

    return S_OK;
  }

  Status next_token() {
    Status ret = _next_token();
    C_TRACE_PARSER("token type: " << token << " token body: '" << token_body()
                                  << "'");
    return ret;
  }
};

struct Interp;

typedef Status(cmd_func_t)(Interp &i, std::vector<string> &argv,
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

  Interp() : commands(0) {}

  //
  ////// COMMANDS & VARIABLES
  //

  Cmd *get_command(const string &name) const {
    for (Cmd *c = commands; c != nullptr; c = c->next) {
      if (c->name.compare(name) == 0) {
        return c;
      }
    }
    return nullptr;
  }

  Status register_command(const string &name, cmd_func_t fn,
                          void *privdata = nullptr) {
    if (get_command(name) != nullptr) {
      std::string err("command already defined: '");
      err += name;
      err += "'";
      result = err;
      return S_ERR;
    }
    Cmd *cmd = new Cmd(name, fn, privdata, commands);
    commands = cmd;
    return S_OK;
  }

  void set_var(const string &name, const string &val) {
    std::cout << "set var" << std::endl;
    // TODO: Implement
  }

  //
  ////// STANDARD LIBRARY
  //

  void register_core_commands() {
    auto puts = [](Interp &i, std::vector<string> &argv, void *privdata) {
      if (argv.size() != 2) {
        return S_ERR;
      }
      std::cout << argv[1] << std::endl;
      return S_OK;
    };

    auto set = [](Interp &i, std::vector<string> &argv, void *privdata) {
      if (argv.size() != 3) {
        return S_ERR;
      }
      i.set_var(argv[1], argv[2]);
      return S_OK;
    };

    register_command("puts", puts);

    register_command("set", set);
  }

  //
  ////// EVALUATION
  //

  Status eval(const string &str) {
    result = "";
    Parser p(str);
    Status ret = S_OK;

    // Tracks command and argument
    std::vector<string> argv;

    while (1) {
      // Previous token type -- note that the parser default value (TK_EOL) is
      // load bearing
      TokenType prevtype = p.token;

      ret = p.next_token();
      // Exit if there's a parser error
      if (ret != S_OK) {
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
          if ((c = get_command(argv[0])) == nullptr) {
            std::string err("command not found: '");
            err += argv[0];
            err += '"';
            result = err;
            return S_ERR;
          }

          c->func(*this, argv, c->privdata);
        }

        // Clear arg vector for the next run and continue
        argv.clear();
        continue;
      }

      // If last token was a separator or EOL, push the final token body
      // to the argument vector and continue
      if (prevtype == TK_SEP || prevtype == TK_EOL) {
        argv.push_back(p.token_body());
      } else {
        std::cout << "interpolation woah" << std::endl;
      }
    }
    return S_OK;
  }
};

} // namespace cacl

#undef C_TRACE_PARSER
#undef C_TRACE_EVAL

#endif