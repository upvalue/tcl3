// tcl.hpp

// C++ reimplementation/butchery of picol

// - Some differences: uses STL string and string_view -- less manual allocs and
//   less allocs overall.
// - Parser has been rewritten to have one main lexing loop
// - Parser interface also uses string_view, doesn't expose as many variables
// - Procedure private data uses virtual destructors for cleanup

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

typedef TokenType Token;

/*
 * Print a string but escape whitespace
 */
struct escape_string {
  escape_string(const string &s_) : s(s_) {}
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
  Parser(const std::string_view &body_, bool trace_parser_ = false)
      : body(body_), trace_parser(trace_parser_) {}
  ~Parser() {}

  const std::string_view body;
  size_t cursor = 0;
  size_t begin = 0, end = 0;
  bool trace_parser;

  bool in_string = false;
  bool in_brace = false;
  bool in_quote = false;
  bool in_command = false;

  size_t brace_level = 0;
  Token token = TK_EOL;
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
      if (c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == ';') {
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

  Token _next_token() {
    int adj = 0;
  start:
    if (done()) {
      if (token != TK_EOL && token != TK_EOF) {
        token = TK_EOL;
      } else {
        token = TK_EOF;
      }
      return token;
    }
    token = TK_ESC;
    begin = cursor;
    while (!done()) {
      adj = 0;
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

        // We're in a quote and this variable is "terminating" another part of
        // the quote. Finish that off first before continuing to parse the
        // variable
        if (in_quote && cursor != begin + 1) {
          back();
          goto finish;
        }

        begin++;
        token = TK_VAR;

        // Variables are not actually a string but we treat them as such
        // to give them the same lexical behavior
        in_string = true;
        break;
      }
      // Potentially a comment
      case '#': {
        if (in_string || in_quote || in_brace)
          continue;
        // Consume until newline
        while (!done()) {
          if (getc() == '\n')
            break;
        }
        goto start;
      }
      case '"': {
        if (in_quote) {
          in_quote = false;
          // Skip closing "
          adj = 1;
          goto finish;
        }
        in_quote = true;
        begin++;
        adj = 1;
      }
      case ' ':
      case '\n':
      case '\r':
      case '\t':
      case ';':
        // If we're in a multiline token or quote, we don't want to break out of
        // the loop, this becomes part of the token because it may be
        // significant whitespace
        if (in_brace) {
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
        // This must come after the string check because we could be parsing
        // a variable within a string
        if (in_quote) {
          continue;
        }
        token = (c == '\n' || c == ';') ? TK_EOL : TK_SEP;
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

  Token next_token() {
    Token t = _next_token();
    if (trace_parser) {
      std::cerr << "{" << "\"type\": \"" << token << "\","
                << " \"begin\": " << begin << "," << " \"end\": " << end << ","
                << " \"body\": \"" << escape_string(token_body()) << "\""
                << "}" << std::endl;
    }
    return t;
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
  Cmd(const string &name_, cmd_func_t *func_, Privdata *privdata_)
      : name(name_), func(func_), privdata(privdata_) {}
  ~Cmd() {
    if (privdata)
      delete privdata;
  }

  string name;
  cmd_func_t *func;
  Privdata *privdata;
};

struct Var {
  string *name, *val;

  ~Var() {
    delete name;
    delete val;
  }
};

struct CallFrame {
  ~CallFrame() {
    for (Var *v : vars) {
      delete v;
    }
  }
  std::vector<Var *> vars;
};

inline Status call_proc(Interp &i, std::vector<string> &argv,
                        Privdata *privdata);

struct Interp {
  std::vector<Cmd *> commands;
  std::vector<CallFrame *> callframes;
  string result;
  bool trace_parser;

  Interp() : trace_parser(false) { callframes.push_back(new CallFrame()); }

  ~Interp() {
    for (CallFrame *cf : callframes) {
      delete cf;
    }

    for (Cmd *c : commands) {
      delete c;
    }
  }

  //
  ////// COMMANDS & VARIABLES
  //

  void drop_call_frame() {
    CallFrame *cf = callframes.back();
    callframes.pop_back();
    delete cf;
  }

  Cmd *get_command(const string &name) const {
    // Is there a better way to look this up in the vector?
    for (Cmd *c : commands) {
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
    commands.push_back(new Cmd(name, fn, privdata));
    return S_OK;
  }

  /**
   * Get a variable by name
   */
  Var *get_var(const std::string_view &name) {
    for (Var *v : callframes.back()->vars) {
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
      callframes.back()->vars.push_back(v);
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
    // Parser p(str, trace_parser);
    Parser p(str, trace_parser);
    Status ret = S_OK;

    // Tracks command and argument
    std::vector<string> argv;

    while (1) {
      // Previous token type -- note that the parser default value (TK_EOL) is
      // load bearing
      TokenType prevtype = p.token;

      Token token = p.next_token();

      std::string_view t = p.token_body();

      // Exit if we're at EOF
      if (token == TK_EOF) {
        break;
      } else if (token == TK_VAR) {
        Var *v = get_var(t);
        if (v == nullptr) {
          C_ERR("variable not found: '" << t << "'");
          return S_ERR;
        }
        t = *v->val;
      } else if (token == TK_CMD) {
        ret = eval(t);
        if (ret != S_OK) {
          return ret;
        }
        t = result;
      } else if (token == TK_SEP) {
        prevtype = token;
        continue;
      } else if (token == TK_EOL) {
        // Once we hit EOL, we should have a command to evaluate
        Cmd *c;
        prevtype = token;

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
      prevtype = token;
    }
    return S_OK;
  }
};

inline Status call_proc(Interp &i, std::vector<string> &argv, Privdata *pd_) {
  ProcPrivdata *pd = static_cast<ProcPrivdata *>(pd_);
  // Set up a new call frame
  CallFrame *cf = new CallFrame();
  i.callframes.push_back(cf);

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