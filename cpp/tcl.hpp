// tcl.hpp

// C++ reimplementation/butchery of picol

// Some differences: uses STL string and string_view -- less allocs
// Parser interface also uses string_view, doesn't expose as many variables
// Procedure private data uses virtual destructors for cleanup

#ifndef _TCL_HPP
#define _TCL_HPP

#include <cstring>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace tcl {

// Typedef std::string to make it easier to experiment with drop in replacements
typedef std::string string;
typedef std::string_view string_view;

enum Status { S_OK = 0, S_ERR = 1, S_RETURN = 2, S_BREAK = 3, S_CONTINUE = 4 };

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

/*
 * Print a string but escape whitespace
 */
struct escape_string {
  escape_string(const string &s_) : s(s_) {}
  const string &s;
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

#define C_ERR(x)                                                               \
  std::ostringstream _c_err_line_##__LINE__;                                   \
  _c_err_line_##__LINE__ << x;                                                 \
  result = _c_err_line_##__LINE__.str();

#define C_CMD_ERR(i, x)                                                        \
  std::ostringstream _c_cmd_err_line_##__LINE__;                               \
  _c_cmd_err_line_##__LINE__ << x;                                             \
  i.result = _c_cmd_err_line_##__LINE__.str();

struct Parser {
  Parser(const string_view &buffer_, bool trace_parser_)
      : buffer(buffer_), result(""), i(0), start(0), end(0), token(TK_EOL),
        insidequote(false), trace_parser(trace_parser_) {}

  string_view buffer;
  string result;
  bool trace_parser;

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

  Status parse_comment() {
    while (!done() && current() != '\n') {
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
    i += 1; // skip {
    start = i;
    while (!done()) {
      if (current() == '\\') {
        i += 1;
      } else if (current() == '{') {
        level += 1;
      } else if (current() == '}') {
        i += 1;
        level -= 1;
        if (level == 0) {
          token = TK_STR;
          end = i - 1;
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
  start:
    token = TK_STR;
    bool new_word = token == TK_SEP || token == TK_EOL || token == TK_STR;

    if (new_word && current() == '{') {
      return parse_brace();
    }

    // Check whether we need to set inside quote and consume "
    if (new_word && current() == '"') {
      insidequote = true;
      i++;
    }

    start = end = i;
    while (true) {
      if (done()) {
        token = TK_ESC;
        end = i;
        return S_OK;
      }
      switch (current()) {
      case '$':
      case '[': {
        end = i;
        token = TK_ESC;
        return S_OK;
      }
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      case ';': {
        if (!insidequote) {
          end = i;
          token = TK_ESC;
          return S_OK;
        }
        break;
      }
      case '"': {
        // If we're inside quote already this means it's time to escape
        if (insidequote) {
          end = i;
          token = TK_ESC;
          insidequote = false;
          i++;
          return S_OK;
        }
      }
      }

      i += 1;
    }
    return S_OK;
  }

  Status parse_command() {
    int level = 1, blevel = 0;
    i += 1;
    start = i;
    bool skip_last = false;
    while (!done()) {
      if (current() == '[' && blevel == 0) {
        level += 1;
      } else if (current() == ']') {
        level -= 1;
        if (level == 0) {
          skip_last = true;
          break;
        }
      } else if (current() == '\\') {
        i++;
      } else if (current() == '{') {
        blevel++;
      } else if (current() == '}') {
        if (blevel != 0) {
          blevel--;
        }
      }
      i++;
    }

    end = i;
    token = TK_CMD;

    if (current() == ']') {
      i++;
    }

    return S_OK;
  }

  string_view token_body() const {
    return string_view(buffer).substr(start, end - start);
  }

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
      case '#':
        if (token == TK_EOL) {
          parse_comment();
          continue;
        }
        return parse_string();
      case '[':
        return parse_command();
      case '$':
        return parse_var();
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
    if (trace_parser) {
      std::cerr << "{\"type\": \"" << token << "\", \"begin\": " << start
                << ", \"end\": " << end << ", \"body\": \""
                << escape_string(std::string(token_body())) << "\"}"
                << std::endl;
    }
    return ret;
  }
};

struct Interp;

struct Privdata {
  virtual ~Privdata() {}
};

struct ProcPrivdata : Privdata {
  ProcPrivdata(string *args_, string *body_) : args(args_), body(body_) {}

  ~ProcPrivdata() {
    delete args;
    delete body;
  }

  string *args;
  string *body;
};

typedef Status(cmd_func_t)(Interp &i, std::vector<string> &argv,
                           Privdata *privdata);

struct Cmd {
  Cmd(const string &name_, cmd_func_t *func_, Privdata *privdata_, Cmd *next_)
      : name(name_), func(func_), privdata(privdata_), next(next_) {}
  ~Cmd() {}

  string name;
  cmd_func_t *func;
  Privdata *privdata;
  Cmd *next;
};

struct Var {
  string *name, *val;
  Var *next;

  ~Var() {
    delete name;
    delete val;
  }
};

struct CallFrame {
  CallFrame() : vars(nullptr), parent(nullptr) {}
  ~CallFrame() {
    Var *v = vars;
    while (v != nullptr) {
      Var *next = v->next;
      delete v;
      v = next;
    }
  }
  Var *vars;
  CallFrame *parent;
};

inline Status call_proc(Interp &i, std::vector<string> &argv,
                        Privdata *privdata);

struct Interp {
  Cmd *commands;
  string result;
  CallFrame *callframe;
  size_t level;
  bool trace_parser;

  Interp()
      : level(0), commands(nullptr), callframe(new CallFrame()),
        trace_parser(false) {}

  ~Interp() {
    CallFrame *cf = callframe;
    while (cf) {
      CallFrame *next = cf->parent;
      delete cf;
      cf = next;
    }

    for (Cmd *c = commands; c != nullptr; c = c->next) {
      delete c->privdata;
      delete c;
    }
  }

  //
  ////// COMMANDS & VARIABLES
  //

  void drop_call_frame() {
    CallFrame *cf = callframe;
    CallFrame *parent = cf->parent;
    delete cf;
    callframe = parent;
  }

  Cmd *get_command(const string &name) const {
    for (Cmd *c = commands; c != nullptr; c = c->next) {
      if (c->name.compare(name) == 0) {
        return c;
      }
    }
    return nullptr;
  }

  Status register_command(const string &name, cmd_func_t fn,
                          Privdata *privdata = nullptr) {
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
  Var *get_var(const std::string_view &name) {
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
    auto puts = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("puts", argv, 2, 2)) {
        return S_ERR;
      }

      std::cout << argv[1] << std::endl;
      return S_OK;
    };

    auto set = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("set", argv, 3, 3)) {
        return S_ERR;
      }

      i.set_var(argv[1], argv[2]);
      return S_OK;
    };

    register_command("puts", puts);
    register_command("set", set);

    // Flow control and procedures

    auto ifc = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
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

    register_command("if", ifc);

    auto whilec = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("while", argv, 3, 3)) {
        return S_ERR;
      }

      while (1) {
        Status s = i.eval(argv[1]);
        if (s != S_OK) {
          return s;
        }

        if (atoi(i.result.c_str())) {
          s = i.eval(argv[2]);
          if (s == S_CONTINUE || s == S_OK) {
            continue;
          } else if (s == S_BREAK) {
            return S_OK;
          } else {
            return s;
          }
        } else {
          return S_OK;
        }
      }

      return S_OK;
    };

    register_command("while", whilec);

    // break & continue

    auto retcodes = [](Interp &i, std::vector<string> &argv,
                       Privdata *privdata) {
      if (!i.arity_check("retcodes", argv, 1, 1)) {
        return S_ERR;
      }

      if (argv[0].compare("break") == 0) {
        return S_BREAK;
      } else if (argv[0].compare("continue") == 0) {
        return S_CONTINUE;
      }

      return S_OK;
    };

    register_command("break", retcodes);
    register_command("continue", retcodes);

    auto proc = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("proc", argv, 4, 4)) {
        return S_ERR;
      }

      ProcPrivdata *ppd =
          new ProcPrivdata(new string(argv[2]), new string(argv[3]));

      i.register_command(argv[1], call_proc, ppd);

      return S_OK;
    };

    register_command("proc", proc);

    auto ret = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("return", argv, 1, 2)) {
        return S_ERR;
      }
      i.result = argv[1];
      return S_RETURN;
    };

    register_command("return", ret);

    ///// Math handling
    auto math = [](Interp &i, std::vector<string> &argv, Privdata *privdata) {
      if (!i.arity_check("math", argv, 3, 3)) {
        return S_ERR;
      }

      int a, b, c = 0;

      if (!i.int_check("math", argv, 1)) {
        return S_ERR;
      }
      if (!i.int_check("math", argv, 2)) {
        return S_ERR;
      }

      a = atoi(argv[1].c_str());
      b = atoi(argv[2].c_str());

      if (argv[0].compare("+") == 0) {
        c = a + b;
      } else if (argv[0].compare("-") == 0) {
        c = a - b;
      } else if (argv[0].compare("*") == 0) {
        c = a * b;
      } else if (argv[0].compare("/") == 0) {
        c = a / b;
      } else if (argv[0].compare(">") == 0) {
        c = a > b;
      } else if (argv[0].compare("<") == 0) {
        c = a < b;
      } else if (argv[0].compare("==") == 0) {
        c = a == b;
      } else if (argv[0].compare("!=") == 0) {
        c = a != b;
      } else if (argv[0].compare(">=") == 0) {
        c = a >= b;
      } else if (argv[0].compare("<=") == 0) {
        c = a <= b;
      } else {
        C_CMD_ERR(i, "[" << argv[0] << "]: unknown operator");
        return S_ERR;
      }

      i.result = std::to_string(c);

      return S_OK;
    };

    register_command("+", math);
    register_command("-", math);
    register_command("*", math);
    register_command("/", math);
    register_command("==", math);
    register_command("!=", math);
    register_command(">", math);
    register_command("<", math);
    register_command(">=", math);
    register_command("<=", math);
  }

  //
  ////// EVALUATION
  //

  Status eval(const string_view &str) {
    result = "";
    Parser p(str, trace_parser);
    Status ret = S_OK;

    // Tracks command and argument
    std::vector<string> argv;

    size_t iter = 0;
    while (1) {
      iter++;
      // Previous token type -- note that the parser default value (TK_EOL) is
      // load bearing
      TokenType prevtype = p.token;

      ret = p.next_token();

      // Exit if there's a parser error
      if (ret != S_OK) {
        return ret;
      }

      std::string_view t = p.token_body();

      // Exit if we're at EOF
      if (p.token == TK_EOF) {
        break;
      }

      if (p.token == TK_VAR) {
        Var *v = get_var(t);
        if (v == nullptr) {
          C_ERR("variable not found: '" << t << "'");
          return S_ERR;
        }
        t = *v->val;
      } else if (p.token == TK_CMD) {
        ret = eval(t);
        if (ret != S_OK) {
          return ret;
        }
        t = result;
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

          Status s = c->func(*this, argv, c->privdata);
          if (s != S_OK) {
            return s;
          }
        }

        // Clear arg vector for the next run and continue
        argv.clear();
        continue;
      }

      // If last token was a separator or EOL, push the final token body
      // to the argument vector and continue
      if (prevtype == TK_SEP || prevtype == TK_EOL) {
        argv.push_back(std::string(t));
      } else {
        argv[argv.size() - 1] += t;
      }
      prevtype = p.token;
    }
    return S_OK;
  }
};

inline Status call_proc(Interp &i, std::vector<string> &argv, Privdata *pd_) {
  ProcPrivdata *pd = static_cast<ProcPrivdata *>(pd_);
  // Set up a new call frame
  CallFrame *cf = new CallFrame();
  cf->parent = i.callframe;
  i.callframe = cf;

  size_t arity = 0;
  string *alist = pd->args;
  string *body = pd->body;

  // Parse arguments list while checking arity
  size_t j = 0, start = 0;
  for (; j < alist->size(); j++) {
    while (j < alist->size() && alist->at(j) == ' ') {
      j++;
    }

    start = j;

    while (j < alist->size() && alist->at(j) != ' ') {
      j++;
    }

    // Got argument
    i.set_var(alist->substr(start, j - start), argv[arity + 1]);

    arity++;

    if (j >= alist->size())
      break;
  }

  Status s = S_OK;
  if (arity != argv.size() - 1) {
    C_CMD_ERR(i, "wrong number of arguments for " << argv[0] << " got "
                                                  << argv.size() << " expected "
                                                  << arity);
    s = S_ERR;
  } else {
    s = i.eval(*body);
    if (s == S_RETURN) {
      s = S_OK;
    }
  }

  i.drop_call_frame();

  return s;
}

} // namespace tcl

#endif