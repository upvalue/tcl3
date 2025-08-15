pub mod tcl {
    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Token {
        ESC,
        STR,
        CMD,
        VAR,
        SEP,
        EOL,
        EOF,
    }

    #[derive(Clone)]
    pub struct Parser<'a> {
        // Because we want to keep the parser to zero allocations, we need to
        // declare a lifetime for it. The parser won't be allowed to outlive the
        // string. This also makes it possible to safely give sub-strings to sub-parsers.
        body: &'a str,

        cursor: usize,
        begin: usize,
        end: usize,
        token: Token,

        in_string: bool,
        in_quote: bool,
        in_brace: bool,

        terminating_char: u8,
        brace_level: usize,

        trace: bool,
    }

    impl<'a> Parser<'a> {
        pub fn new(body: &'a str) -> Parser<'a> {
            Parser {
                body,

                cursor: 0,
                begin: 0,
                end: 0,

                token: Token::EOL,

                in_string: false,
                in_quote: false,
                in_brace: false,

                terminating_char: 0,
                brace_level: 0,

                trace: true,
            }
        }

        pub fn done(&mut self) -> bool {
            return self.cursor >= self.body.len();
        }

        pub fn peek(&mut self) -> u8 {
            return self.body.chars().nth(self.cursor).unwrap_or('\0') as u8;
        }

        pub fn getc(&mut self) -> u8 {
            let c = self.peek();
            self.cursor += 1;
            return c;
        }

        pub fn back(&mut self) {
            self.cursor -= 1;
        }

        pub fn consume_whitespace(&mut self) {
            while !self.done() {
                let c = self.peek();
                if c == b' ' || c == b'\n' || c == b'\r' || c == b'\t' || c == b';' {
                    self.getc();
                } else {
                    break;
                }
            }
        }

        pub fn recurse(&mut self, sub: &mut Parser, terminating_char: u8) {
            sub.terminating_char = terminating_char;
            while sub.next() != Token::EOF {}
            self.cursor = self.cursor + sub.cursor;
        }

        pub fn next_impl(&mut self) -> Token {
            if self.done() {
                if self.token != Token::EOF && self.token != Token::EOL {
                    self.token = Token::EOL;
                } else {
                    self.token = Token::EOF;
                }
                return self.token;
            }

            self.token = Token::ESC;
            self.begin = self.cursor;
            self.end = self.cursor;

            let mut c: u8;
            let mut adj: usize = 0;

            while !self.done() {
                adj = 0;

                c = self.getc();

                if c == self.terminating_char {
                    return Token::EOF;
                }

                match c {
                    b'{' => {
                        if self.in_quote || self.in_string {
                            continue;
                        }

                        if !self.in_brace {
                            self.begin += 1;
                            self.token = Token::STR;
                            self.in_brace = true;
                        }

                        self.brace_level += 1;
                    }
                    b'}' => {
                        if self.in_quote || self.in_string {
                            continue;
                        }

                        if self.brace_level > 0 {
                            self.brace_level -= 1;
                            if self.brace_level == 0 {
                                self.in_brace = false;
                                adj = 1;
                                break;
                            }
                        }
                    }
                    b'[' => {
                        if self.in_string || self.in_quote || self.in_brace {
                            continue;
                        }

                        let mut sub: Parser = Parser::new(&self.body[self.cursor..]);

                        self.recurse(&mut sub, b']');
                        adj = 1;
                        self.token = Token::CMD;
                        break;
                    }

                    b'$' => {
                        if self.in_string || self.in_brace {
                            continue;
                        }

                        if self.in_quote {
                            if self.cursor != self.begin + 1 {
                                self.back();
                                break;
                            }
                        }

                        self.begin += 1;
                        self.token = Token::VAR;
                        self.in_string = true;
                    }

                    b'#' => {
                        if self.in_string || self.in_quote || self.in_brace {
                            continue;
                        }

                        while !self.done() {
                            if self.getc() == b'\n' {
                                break;
                            }
                        }

                        return self.next_impl();
                    }

                    b'"' => {
                        if self.in_quote {
                            self.in_quote = false;
                            adj = 1;
                            break;
                        }

                        self.in_quote = true;
                        self.begin += 1;
                        adj = 1;
                    }

                    b'\n' | b'\r' | b'\t' | b';' | b' ' => {
                        if self.in_brace {
                            continue;
                        }

                        if self.in_string {
                            self.back();
                            self.in_string = false;
                            break;
                        }

                        if self.in_quote {
                            continue;
                        }

                        self.token = if c == b'\n' || c == b';' {
                            Token::EOL
                        } else {
                            Token::SEP
                        };

                        self.consume_whitespace();
                        break;
                    }
                    _ => {
                        if !self.in_brace && !self.in_quote {
                            self.in_string = true;
                        }
                    }
                }
            }

            self.end = self.cursor - adj;

            self.token
        }

        pub fn token_body(&mut self) -> &str {
            return &self.body[self.begin..self.end];
        }

        pub fn next(&mut self) -> Token {
            let tk = self.next_impl();

            if self.trace {
                let begin = self.begin;
                let end = self.end;
                eprintln!(
                    "{{\"token\": \"TK_{:?}\", \"begin\": {}, \"end\": {}, \"body\": {:?}}}",
                    tk,
                    begin,
                    end,
                    self.token_body()
                );
            }

            tk
        }

        pub fn set_trace(&mut self, trace: bool) {
            self.trace = trace;
        }
    }
}
