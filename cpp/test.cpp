#include <cstring>
#include <string>

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "vendor/doctest/doctest.h"

#include "cacl.hpp"

using namespace cacl;

TEST_CASE("empty") { CHECK(0 == 0); }

void checkSep(Parser &p) {
  CHECK(p.getToken() == C_OK);
  CHECK(p.tokenType() == T_SEP);
}

TEST_CASE("parser parses separators") {
  Parser p1(" ");
  checkSep(p1);

  Parser p2(" \t\n\r");
  checkSep(p2);

  Parser p3(" \t\n\r\t\n\r");
  checkSep(p3);
}

void checkEol(Parser &p) {
  CHECK(p.getToken() == C_OK);
  CHECK(p.tokenType() == T_EOL);
}

TEST_CASE("parser parses EOL") {
  Parser p1("\n");
  checkEol(p1);

  Parser p2(";");
  checkEol(p2);
}

TEST_CASE("parser parses plain string") {
  Parser p1("\"12345\"");
  CHECK(p1.getToken() == C_OK);
  CHECK(p1.tokenType() == T_ESC);
}

TEST_CASE("parser parses plain command") {
  Parser p1("testret\n");
  CHECK(p1.getToken() == C_OK);
  CHECK(p1.tokenType() == T_EOL);
}