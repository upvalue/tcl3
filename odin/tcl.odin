package main

import "core:fmt"
import "core:strings"

// Open questions & thoughts:

// Is there an idiomatic way to simplify parser iteration?
//   Needs to be stateful and support multiple invocation, but
//   getting rid of the string would be nice

// Odin supports multiple returns which can probably help remove
// some state in Parser

// Memory management?

// Test?

Token :: enum {
	UNSET,
	EOL,
	ESC,
	STR,
	CMD,
	VAR,
	SEP,
	EOF,
}

Ret :: enum {
	OK,
	ERR,
}

Parser :: struct {
	token: Token,
	idx:   int,
	str:   string,
	sb:    strings.Builder,
}

State :: struct {
	p: Parser,
}

eat_newline :: proc(p: ^Parser) {
	for ; p.idx != len(p.str); p.idx += 1 {
		return
	}
}

parse_eol :: proc(p: ^Parser) -> Ret {
	for p.idx != len(p.str) &&
	    (p.str[p.idx] == ' ' ||
			    p.str[p.idx] == '\t' ||
			    p.str[p.idx] == '\r' ||
			    p.str[p.idx] == ';') {
		p.idx += 1
	}
	p.token = Token.EOL
	return Ret.OK
}

parse_string :: proc(p: ^Parser) -> Ret {
	// Track whether we're inside a quote
	// Track string beginning and end
	// Backslashes do ?
	// $ and [ are escapes
	// Whitespace and semicolons terminate strings unless inside quote
	// Track quote when in end of string
	strings.builder_init_len(&p.sb, 512)
	for ; p.idx != len(p.str); p.idx += 1 {
		if p.str[p.idx] == ' ' {
			break
		}
		strings.write_byte(&p.sb, p.str[p.idx])
	}
	p.token = Token.STR
	fmt.printf("got string '%v'\n", strings.to_string(p.sb))
	return Ret.OK
}

parse_comment :: proc(p: ^Parser) -> Ret {
	for ; p.idx != len(p.str) && p.str[p.idx] != '\n'; p.idx += 1 {
	}
	return Ret.OK
}

next_token :: proc(p: ^Parser) -> Ret {
	token_iter: for ; p.idx != len(p.str); p.idx += 1 {
		c := p.str[p.idx]

		fmt.printf("next token iter: %c '%v'\n", c, c)

		switch c {
		// Handle whitespace
		case ' ':
			fallthrough;case '\r':
			fallthrough;case '\t':
			fmt.printf("ws %v\n", c)
			continue token_iter

		// Handle end of line
		case '\n':;case ':':
			return parse_eol(p)

		case '#':
			return parse_comment(p)

		case:
			return parse_string(p)
		}

		fmt.printf("unknown char '%c' %v\n", c, c)
		return Ret.ERR
	}

	p.token = Token.EOF
	return Ret.OK
}


// Evaluate an arbitrary string
eval :: proc(s: ^State, str: string) {
	p := Parser {
		str = str,
	}

	for p.token != Token.EOF {
		fmt.printf("loop iter\n")
		ret := next_token(&p)

		if ret == Ret.ERR {
			fmt.printf("Stopped due to error\n")
			break

		}
	}

	fmt.printf("Stopped due to reaching EOF\n")
}

state_init :: proc(s: ^State) {
	s.p = Parser{}
}

main :: proc() {
	s := State{}
	state_init(&s)
	// mystring := "puts 12345\n"
	mystring := "# meowdy pal"
	eval(&s, mystring)
}
