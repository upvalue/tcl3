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

    pub struct Parser {
        body: String,

        cursor: usize,
        begin: usize,
        end: usize,
        token: Token,

        in_string: bool,
        in_quote: bool,
        in_command: bool,
        in_brace: bool,

        brace_level: usize,

        trace: bool,
    }

    impl Parser {
        pub fn new(body: String) -> Parser {
            Parser {
                body,

                cursor: 0,
                begin: 0,
                end: 0,

                token: Token::EOL,

                in_string: false,
                in_quote: false,
                in_command: false,
                in_brace: false,

                brace_level: 0,

                trace: true,
            }
        }

        pub fn done(self: &mut Parser) -> bool {
            println!("done: {}", self.cursor >= self.body.len());
            return self.cursor >= self.body.len();
        }

        pub fn peek(self: &mut Parser) -> u8 {
            return self.body.chars().nth(self.cursor).unwrap_or('\0') as u8;
        }

        pub fn getc(self: &mut Parser) -> u8 {
            let c = self.peek();
            self.cursor += 1;
            return c;
        }

        pub fn back(self: &mut Parser) {
            self.cursor -= 1;
        }

        pub fn consume_whitespace(self: &mut Parser) {
            while !self.done() {
                let c = self.peek();
                if c == b' ' || c == b'\n' || c == b'\r' || c == b'\t' || c == b';' {
                    self.getc();
                } else {
                    break;
                }
            }
        }

        /*
        pub fn recurse(self: &mut Parser, sub: &mut Parser, terminating_char: u8) {
            sub.terminating_char = terminating_char;
            while sub.next() != Token::EOF {}
            self.cursor = self.cursor + sub.cursor;
        }
        */

        pub fn next_impl(self: &mut Parser) -> Token {
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

            let mut c: u8 = 0;
            let mut adj: usize = 0;

            while !self.done() {
                adj = 0;

                c = self.getc();

                match c {
                    b'$' => {
                        if self.in_string {
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
                        return self.token;
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

        pub fn token_body(self: &mut Parser) -> &str {
            return &self.body[self.begin..self.end];
        }

        pub fn next(self: &mut Parser) -> Token {
            let tk = self.next_impl();

            if self.trace {
                eprintln!(
                    "{{\"token\": \"TK_{:?}\", \"body\": \"{}\"}}",
                    tk,
                    self.token_body()
                );
            }

            tk
        }
    }
}
