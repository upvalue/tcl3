#include <cstring>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest.h"

#include "cacl.hpp"

using namespace cacl;

void checkSep(Parser &p) {
  CHECK(p.nextToken() == C_OK);
  CHECK(p.tokenType() == TK_SEP);
}

/*
TEST_CASE("parser parses separators") {
  Parser p1(" ");
  checkSep(p1);

  Parser p2(" \t\n\r");
  checkSep(p2);

  Parser p3(" \t\n\r\t\n\r");
  checkSep(p3);
}

void checkEol(Parser &p) {
  CHECK(p.nextToken() == C_OK);
  CHECK(p.tokenType() == TK_EOL);
}

TEST_CASE("parser parses EOL") {
  Parser p1("\n");
  checkEol(p1);

  Parser p2(";");
  checkEol(p2);
}

TEST_CASE("parser parses plain string") {
  Parser p1("\"12345\"");
  CHECK(p1.nextToken() == C_OK);
  CHECK(p1.tokenType() == TK_ESC);
}

TEST_CASE("parser parses plain command") {
  Parser p1("testret\n");
  CHECK(p1.nextToken() == C_OK);
  CHECK(p1.tokenType() == TK_ESC);
}
  */

///// EVALUATOR TESTS

ReturnCode test_ret(Interp *i, int argc, char **argv, void *privdata) {
  return C_OK;
}

/*
TEST_CASE("defining duplicate commands causes error") {
  Interp i;
  CHECK(i.register_command("testret", test_ret) == C_OK);
  CHECK(i.register_command("testret", test_ret) == C_ERR);
}
  */

TEST_CASE("evaluator handles nonexistent command") {
  Interp i;
  i.eval("notreal");
  CHECK(i.eval("notreal") == C_ERR);
}

/*
TEST_CASE("evaluator evaluates basic command") {
  Interp i;
  i.register_command("testret", 0);
  CHECK(i.eval("testret\n") == C_OK);
  CHECK(i.result.compare("success") == 0);
}
  */