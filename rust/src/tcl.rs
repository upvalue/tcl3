pub mod tcl {
    use std::any::Any;

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

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Status {
        OK,
        ERROR,
        BREAK,
        CONTINUE,
        RETURN,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum TclError {
        General,
        Arity,
        CommandNotFound,
        CommandAlreadyDefined,
        VariableNotFound,
    }

    pub struct Parser<'a> {
        // Because we want to keep the parser to zero allocations, we need to
        // declare a lifetime here so we can simply take a reference to a string
        // instead of having our own heap-allocated string.  The parser won't be
        // allowed to outlive the string. This also makes it possible to safely
        // give sub-strings to sub-parsers.
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

                        self.begin += 1;
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

    trait Privdata: Any {
        fn finalize(self: Box<Self>);
    }

    #[derive(Clone, Debug)]
    struct ProcPrivdata {
        args: Box<String>,
        body: Box<String>,
    }

    impl Privdata for ProcPrivdata {
        fn finalize(mut self: Box<Self>) {
            self.args.clear();
            self.body.clear();
            // drop(self);
        }
    }

    struct Var {
        name: String,
        value: String,
    }

    struct CallFrame {
        vars: Vec<Var>,
    }

    impl CallFrame {
        pub fn new() -> CallFrame {
            CallFrame { vars: Vec::new() }
        }

        pub fn set_var(&mut self, name: &str, value: &str) -> Result<Status, TclError> {
            for var in self.vars.iter_mut() {
                if var.name == name {
                    var.value = value.to_string();
                    return Ok(Status::OK);
                }
            }
            self.vars.push(Var {
                name: name.to_string(),
                value: value.to_string(),
            });
            Ok(Status::OK)
        }
    }

    pub struct Cmd {
        name: String,
        cmd_func:
            fn(&mut Interp, &Vec<String>, Option<Box<dyn Privdata>>) -> Result<Status, TclError>,
        privdata: Option<Box<dyn Privdata>>,
    }

    pub struct Interp {
        commands: Vec<Cmd>,
        callframes: Vec<CallFrame>,
        pub result: Option<String>,
    }

    fn check_arity(
        interp: &mut Interp,
        argv: &Vec<String>,
        min: usize,
        max: usize,
    ) -> Result<Status, TclError> {
        if argv.len() < min || argv.len() > max {
            interp.result = Some(format!(
                "wrong number of arguments to {name}: expected {min}-{max}, got {len}",
                name = argv[0],
                min = min,
                max = max,
                len = argv.len()
            ));
            return Err(TclError::Arity);
        }
        Ok(Status::OK)
    }

    fn cmd_puts(
        interp: &mut Interp,
        argv: &Vec<String>,
        _privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 2, 2)?;

        println!("{}\n", argv[1]);
        Ok(Status::OK)
    }

    fn cmd_set(
        interp: &mut Interp,
        argv: &Vec<String>,
        _privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 3, 3)?;
        interp.set_var(&argv[1], &argv[2])?;
        Ok(Status::OK)
    }

    fn call_proc(
        interp: &mut Interp,
        argv: &Vec<String>,
        privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        // check_arity(interp, argv, 0, 0)?;

        let cf = CallFrame::new();
        interp.callframes.push(cf);

        let mut arity = 0;
        // defer interp.drop_call_frame();

        /*let ppd: *ProcPrivdata = @alignCast(@ptrCast(privdata.?.data));
        const cf = CallFrame.init(i.allocator);
        try i.callframes.append(cf);
        defer i.drop_call_frame();*/
        Ok(Status::OK)
    }

    fn cmd_proc(
        interp: &mut Interp,
        argv: &Vec<String>,
        _privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 4, 4)?;

        let ppd = Box::new(ProcPrivdata {
            args: Box::new(argv[2].clone()),
            body: Box::new(argv[3].clone()),
        });

        interp.register_command(&argv[1], call_proc, Some(ppd))?;

        Ok(Status::OK)
    }

    fn cmd_return(
        interp: &mut Interp,
        argv: &Vec<String>,
        _privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 1, 1)?;
        interp.result = Some(argv[1].clone());
        Ok(Status::RETURN)
    }

    fn cmd_math(
        interp: &mut Interp,
        argv: &Vec<String>,
        _privdata: Option<Box<dyn Privdata>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 3, 3)?;

        // Hadnle errors
        let achk = argv[1].parse::<i64>();
        if achk.is_err() {
            interp.result = Some(format!("invalid number: '{}'", argv[1]));
            return Err(TclError::General);
        }

        let bchk = argv[2].parse::<i64>();

        if bchk.is_err() {
            interp.result = Some(format!("invalid number: '{}'", argv[2]));
            return Err(TclError::General);
        }

        let a = achk.unwrap();
        let b = bchk.unwrap();

        match argv[0].as_str() {
            "+" => interp.result = Some(format!("{}", a + b)),
            "-" => interp.result = Some(format!("{}", a - b)),
            "*" => interp.result = Some(format!("{}", a * b)),
            "/" => interp.result = Some(format!("{}", a / b)),
            ">" => interp.result = Some(format!("{}", a > b)),
            "<" => interp.result = Some(format!("{}", a < b)),
            "==" => interp.result = Some(format!("{}", a == b)),
            "!=" => interp.result = Some(format!("{}", a != b)),
            ">=" => interp.result = Some(format!("{}", a >= b)),
            "<=" => interp.result = Some(format!("{}", a <= b)),
            _ => {
                interp.result = Some(format!("unknown operator: '{}'", argv[1]));
                return Err(TclError::General);
            }
        }

        Ok(Status::OK)
    }

    impl Interp {
        pub fn new() -> Interp {
            let mut interp = Interp {
                commands: Vec::new(),
                callframes: Vec::new(),
                result: None,
            };
            interp.callframes.push(CallFrame::new());
            interp
        }

        pub fn set_var(&mut self, name: &str, value: &str) -> Result<Status, TclError> {
            let callframe = self.callframes.last_mut().unwrap();
            callframe.set_var(name, value)?;
            Ok(Status::OK)
        }

        pub fn get_var(&self, name: &str) -> Option<&Var> {
            let callframe = self.callframes.last().unwrap();
            for var in callframe.vars.iter() {
                if var.name == name {
                    return Some(var);
                }
            }
            None
        }

        pub fn get_command(&self, name: &str) -> Option<&Cmd> {
            self.commands.iter().find(|c| c.name == name)
        }

        pub fn register_command(
            &mut self,
            name: &str,
            cmd: fn(
                &mut Interp,
                &Vec<String>,
                Option<Box<dyn Privdata>>,
            ) -> Result<Status, TclError>,
            privdata: Option<Box<dyn Privdata>>,
        ) -> Result<Status, TclError> {
            if self.get_command(name).is_some() {
                self.result = Some(format!("command already defined: '{name}'", name = name));
                return Err(TclError::CommandAlreadyDefined);
            }

            let cmd = Cmd {
                name: name.to_string(),
                cmd_func: cmd,
                privdata: privdata,
            };

            self.commands.push(cmd);

            Ok(Status::OK)
        }

        pub fn register_core_commands(&mut self) {
            let _ = self.register_command("puts", cmd_puts, None);
            let _ = self.register_command("set", cmd_set, None);
            let _ = self.register_command("proc", cmd_proc, None);
            let _ = self.register_command("return", cmd_return, None);

            let _ = self.register_command("+", cmd_math, None);
            let _ = self.register_command("-", cmd_math, None);
            let _ = self.register_command("*", cmd_math, None);
            let _ = self.register_command("/", cmd_math, None);
            let _ = self.register_command(">", cmd_math, None);
            let _ = self.register_command("<", cmd_math, None);
            let _ = self.register_command(">=", cmd_math, None);
            let _ = self.register_command("<=", cmd_math, None);
            let _ = self.register_command("==", cmd_math, None);
            let _ = self.register_command("!=", cmd_math, None);
        }

        pub fn eval(&mut self, str: &str) -> Result<Status, TclError> {
            // TODO do the rest of this thing
            let mut p = Parser::new(str);

            let mut argv: Vec<String> = Vec::new();
            loop {
                let prevtype = p.token;
                let token = p.next();
                let mut t = p.token_body();

                if token == Token::EOF {
                    break;
                } else if token == Token::VAR {
                    let var = self.get_var(&t);
                    if var.is_some() {
                        t = &var.unwrap().value;
                    } else {
                        self.result = Some(format!("variable not found: '{name}'", name = t));
                        return Err(TclError::VariableNotFound);
                    }
                } else if token == Token::CMD {
                    let ret = self.eval(t);
                    if ret.is_err() || ret.unwrap() != Status::OK {
                        return ret;
                    }
                    t = self.result.as_ref().unwrap();
                } else if token == Token::SEP {
                    continue;
                } else if token == Token::EOL {
                    if !argv.is_empty() {
                        let cmd_name = &argv[0];
                        println!("cmd_name: '{cmd_name}'", cmd_name = cmd_name);
                        let cmd = self.get_command(cmd_name);
                        if cmd.is_some() {
                            let res =
                                (cmd.unwrap().cmd_func)(self, &argv, cmd.unwrap().privdata.clone());
                            if (res.is_ok() && res.ok().unwrap() != Status::OK) || res.is_err() {
                                return res;
                            }
                        } else {
                            self.result = Some(format!(
                                "command not found: '{cmd_name}'",
                                cmd_name = cmd_name
                            ));
                            return Err(TclError::CommandNotFound);
                        }
                    }
                    argv.clear();

                    continue;
                }

                if prevtype == Token::SEP || prevtype == Token::EOL {
                    // dup string
                    let duped = t.to_string();
                    argv.push(duped);
                } else {
                    // append to prev token
                    let prev = argv.last().unwrap();
                    let new = format!("{}{}", prev, t);
                    argv.pop();
                    argv.push(new);
                }
            }
            Ok(Status::OK)
        }
    }
}
