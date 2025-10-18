#include "tcl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "argh.h"
#include "linenoise.h"

using namespace tcl;

// Simple helper to check if a flag is allowed
bool is_allowed_flag(const char *flag) {
  return strcmp(flag, "t") == 0 || strcmp(flag, "trace-parser") == 0 ||
         strcmp(flag, "p") == 0 || strcmp(flag, "parser-only") == 0 ||
         strcmp(flag, "h") == 0 || strcmp(flag, "help") == 0;
}

/**
 * Read entire file into a string
 */
string read_file(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return string();
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buffer = (char *)malloc(size + 1);
  size_t read = fread(buffer, 1, size, f);
  buffer[read] = '\0';
  fclose(f);

  string result(buffer);
  free(buffer);
  return result;
}

/**
 * Handles executing Tcl code or just parsing it
 */
void exec(Interp &i, const string &content, bool eval) {
  if (eval) {
    Status s = i.eval(string_view(content));
    if (s != S_OK) {
      fprintf(stderr, "Error evaluating file: %s\n", i.result.c_str());
      return;
    }
  } else {
    string_view sv(content);
    Parser p(sv);
    p.trace_parser = i.trace_parser;
    while (true) {
      Token t = p.next_token();
      if (t == TK_EOF) {
        break;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  argh::parser cmdl(argv);
  bool trace_parser = false;
  bool parser_only = false;

  for (const auto &flag : cmdl.flags()) {
    if (!is_allowed_flag(flag.c_str())) {
      fprintf(stderr, "Unknown flag: %s\n", flag.c_str());
      return 1;
    }
  }

  if (cmdl[{"-h", "--help"}]) {
    printf("Usage: repl [options] [file]\n");
    printf("Options:\n");
    printf("  -t, --trace-parser   Enable parser tracing\n");
    printf("  -p, --parser-only    Only parse the input, don't execute\n");
    printf("  -h, --help           Show this help message\n");
    printf("If no file is given, the REPL will start.\n");
    return 0;
  }

  if (cmdl[{"-t", "--trace-parser"}]) {
    trace_parser = true;
  }

  if (cmdl[{"-p", "--parser-only"}]) {
    parser_only = true;
  }

  Interp i;
  i.trace_parser = trace_parser;
  register_core_commands(i);

  if (cmdl(1)) {
    FILE *test = fopen(cmdl[1].c_str(), "r");
    if (!test) {
      fprintf(stderr, "Error: Cannot open file '%s'\n", cmdl[1].c_str());
      return 1;
    }
    fclose(test);

    string content = read_file(cmdl[1].c_str());
    exec(i, content, !parser_only);
  } else {
    while (true) {
      char *line = linenoise("tcl> ");
      if (!line) {
        break;
      }
      string ln(line);
      free(line);
      if (ln.empty()) {
        break;
      }
      exec(i, ln, !parser_only);
    }
  }
  return 0;
}
