package main

import "core:fmt"

State :: struct {

}

Token :: enum {
  EOL,
  ESC,
  STR,
  CMD,
  VAR,
  SEP,
  EOF
}

Parser :: struct {
  token: Token

}

ocl_next_token :: proc(p: ^Parser, str: string) {


}


// Evaluate an arbitrary string
ocl_eval :: proc(str: string) {
  p := Parser{}

  ocl_next_token(&p, str)


}


main :: proc() {
  mystring := "puts 12345"
  ocl_eval(mystring)
}

