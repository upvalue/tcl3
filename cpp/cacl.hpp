#ifndef _CACL_HPP
#define _CACL_HPP

#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
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

#define C_ERR(x)                                                               \
  std::ostringstream _c_err_line_##__LINE__;                                   \
  _c_err_line_##__LINE__ << x;                                                 \
  result = _c_err_line_##__LINE__.str();

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
   * Brace -- finds the matching brace
   * while handling levels
   */
  Status parse_brace() {
    size_t level = 1;
    Save save(this);
    while (!done()) {
      if (current() == '\\') {
        i += 1;
      } else if (current() == '{') {
        level += 1;
      } else if (current() == '}') {
        level -= 1;
        if (level == 0) {
          token = TK_STR;
          return S_OK;
        }
      }
      i += 1;
    }
    // Not reached -- but in part because we don't
    // complain about brace mismatches
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

// TODO: can privdata be replaced with lambda capture?
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

struct Var {
  std::string *name, *val;
  Var *next;
};

struct CallFrame {
  CallFrame() : vars(nullptr), parent(nullptr) {}
  ~CallFrame() {
    for (Var *v = vars; v != nullptr; v = v->next) {
      delete v->name;
      delete v->val;
      delete v;
    }
  }
  Var *vars;
  CallFrame *parent;
};

struct Interp {
  Cmd *commands;
  string result;
  CallFrame *callframe;
  size_t level;

  Interp() : level(0), commands(nullptr), callframe(new CallFrame()) {}

  ~Interp() {
    /*
    TODO segfault and cleanup/
    for (CallFrame *cf = callframe; cf != nullptr; cf = cf->parent) {
      delete cf;
    }
      */
  }

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

  /**
   * Get a variable by name
   */
  Var *get_var(const string &name) {
    for (Var *v = callframe->vars; v != nullptr; v = v->next) {
      if (v->name->compare(name) == 0) {
        return v;
      }
    }
    return nullptr;
  }

  /**
   * Set a variable by name
   */
  Status set_var(const string &name, const string &val) {
    Var *v = get_var(name);
    if (v) {
      delete v->val;
      v->val = new string(val);
    } else {
      v = new Var();
      v->name = new string(name);
      v->val = new string(val);
      v->next = callframe->vars;
      callframe->vars = v;
    }
    return S_OK;
  }

  //
  ////// STANDARD LIBRARY
  //

  bool arity_check(const string &name, const std::vector<string> &argv,
                   size_t min, size_t max) {
    if (min == max && argv.size() != min) {
      C_ERR("wrong number of args for " << name << " (expected " << min << ")");
      return false;
    }

    if (argv.size() < min || argv.size() > max) {
      C_ERR("[" << name << "]: wrong number of args (expected " << min << " to "
                << max << ")");
      return false;
    }
    return true;
  }

  /**
   * Check whether an argument to a function is a valid integer
   */
  bool int_check(const string &name, const std::vector<string> &argv,
                 size_t idx) {
    const string &arg = argv[idx];
    for (size_t i = 0; i < arg.length(); i++) {
      if (!isdigit(arg[i])) {
        C_ERR("[" << name << "]: argument " << idx << " is not an integer");
        return false;
      }
    }
    return true;
  }

  void register_core_commands() {
    auto puts = [](Interp &i, std::vector<string> &argv, void *privdata) {
      if (!i.arity_check("puts", argv, 2, 2)) {
        return S_ERR;
      }

      std::cout << argv[1] << std::endl;
      return S_OK;
    };

    auto set = [](Interp &i, std::vector<string> &argv, void *privdata) {
      if (!i.arity_check("set", argv, 3, 3)) {
        return S_ERR;
      }

      i.set_var(argv[1], argv[2]);
      return S_OK;
    };

    auto ifc = [](Interp &i, std::vector<string> &argv, void *privdata) {
      if (!i.arity_check("if", argv, 3, 5)) {
        return S_ERR;
      }

      // Evaluate condition
      if (i.eval(argv[1]) != S_OK) {
        return S_ERR;
      }

      // Branch condition

      if (atoi(i.result.c_str())) {
        return i.eval(argv[2]);
      } else if (argv.size() == 5) {
        return i.eval(argv[4]);
      }
      return S_OK;
    };

    register_command("puts", puts);
    register_command("set", set);
    register_command("if", ifc);
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
            C_ERR("command not found: '" << argv[0] << "'");
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