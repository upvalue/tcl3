// Most explanatory comments are in the C++ version.
pub mod tcl {
    use std::any::Any;
    use std::rc::Rc;

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Token {
        Esc,
        Str,
        Cmd,
        Var,
        Sep,
        Eol,
        Eof,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum Status {
        Ok,
        Return,
        Break,
        Continue,
    }

    #[derive(Clone, Copy, Debug, PartialEq, Eq)]
    pub enum TclError {
        General,
        Arity,
        CommandNotFound,
        CommandAlreadyDefined,
        VariableNotFound,
        InvalidNumber,
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

                token: Token::Eol,

                in_string: false,
                in_quote: false,
                in_brace: false,

                terminating_char: 0,
                brace_level: 0,

                trace: false,
            }
        }

        pub fn done(&mut self) -> bool {
            self.cursor >= self.body.len()
        }

        pub fn peek(&mut self) -> u8 {
            self.body.as_bytes()[self.cursor]
        }

        pub fn getc(&mut self) -> u8 {
            let c = self.peek();
            self.cursor += 1;
            c
        }

        pub fn back(&mut self) {
            self.cursor -= 1;
        }

        pub fn consume_whitespace_check_eol(&mut self) -> bool {
            while !self.done() {
                let c = self.peek();
                if c == b'\n' {
                    return true;
                } else if c == b' ' || c == b'\r' || c == b'\t' || c == b';' {
                    self.getc();
                } else {
                    break;
                }
            }
            false
        }

        pub fn recurse(&mut self, sub: &mut Parser, terminating_char: u8) {
            sub.terminating_char = terminating_char;
            loop {
                let tk = sub.next();
                if tk == Token::Eof {
                    break;
                }
            }
            self.cursor += sub.cursor;
        }

        pub fn next_impl(&mut self) -> Token {
            if self.done() {
                if self.token != Token::Eof && self.token != Token::Eol {
                    self.token = Token::Eol;
                } else {
                    self.token = Token::Eof;
                }
                return self.token;
            }

            self.token = Token::Esc;
            self.begin = self.cursor;

            let mut c: u8;
            let mut adj: usize = 0;

            while !self.done() {
                adj = 0;

                c = self.getc();

                if c == self.terminating_char {
                    self.end = self.cursor - 1;
                    return Token::Eof;
                }

                match c {
                    b'{' => {
                        if self.in_quote || self.in_string {
                            continue;
                        }

                        if !self.in_brace {
                            self.begin += 1;
                            self.token = Token::Str;
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
                        self.token = Token::Cmd;
                        break;
                    }

                    b'$' => {
                        if self.in_string || self.in_brace {
                            continue;
                        }

                        if self.in_quote && self.cursor != self.begin + 1 {
                            self.back();
                            break;
                        }

                        self.begin += 1;
                        self.token = Token::Var;
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
                            Token::Eol
                        } else {
                            Token::Sep
                        };

                        if self.consume_whitespace_check_eol() {
                            self.token = Token::Eol;
                        }
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
            &self.body[self.begin..self.end]
        }

        pub fn next(&mut self) -> Token {
            let tk = self.next_impl();

            if self.trace {
                let begin = self.begin;
                let end = self.end;
                let uppercase_type = format!("{tk:?}").to_uppercase();
                eprintln!(
                    "{{\"type\": \"TK_{uppercase_type}\", \"begin\": {begin}, \"end\": {end}, \"body\": {:?}}}",
                    self.token_body()
                );
            }

            tk
        }
    }

    #[derive(Clone, Debug)]
    struct ProcPrivdata {
        args: String,
        body: String,
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
                    return Ok(Status::Ok);
                }
            }
            self.vars.push(Var {
                name: name.to_string(),
                value: value.to_string(),
            });
            Ok(Status::Ok)
        }
    }

    type CmdFunc = fn(&mut Interp, &[String], Option<Rc<dyn Any>>) -> Result<Status, TclError>;

    pub struct Cmd {
        name: String,
        cmd_func: CmdFunc,
        privdata: Option<Rc<dyn Any>>,
    }

    pub struct Interp {
        commands: Vec<Cmd>,
        callframes: Vec<CallFrame>,
        pub result: Option<String>,
        pub trace_parser: bool,
    }

    fn check_arity(
        interp: &mut Interp,
        argv: &[String],
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
        Ok(Status::Ok)
    }

    fn cmd_puts(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 2, 2)?;

        println!("{}", argv[1]);
        Ok(Status::Ok)
    }

    fn cmd_set(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 3, 3)?;
        interp.set_var(&argv[1], &argv[2])?;
        Ok(Status::Ok)
    }

    fn call_proc(
        interp: &mut Interp,
        argv: &[String],
        privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        let ppd = privdata
            .as_ref()
            .and_then(|p| p.downcast_ref::<ProcPrivdata>())
            .ok_or_else(|| {
                interp.result = Some("internal error: invalid proc privdata".to_string());
                TclError::General
            })?;

        let cf = CallFrame::new();
        interp.callframes.push(cf);

        let alist = &ppd.args;

        let mut start;
        let mut j: usize = 0;
        let mut arity: usize = 0;

        while j < alist.len() {
            while j < alist.len() && alist.as_bytes()[j] == b' ' {
                j += 1;
            }

            start = j;

            while j < alist.len() && alist.as_bytes()[j] != b' ' {
                j += 1;
            }

            interp.set_var(&alist[start..j], &argv[arity + 1])?;

            arity += 1;

            if j >= alist.len() {
                break;
            }
        }

        if arity != argv.len() - 1 {
            interp.result = Some(format!(
                "wrong number of arguments for {}: expected {arity}, got {}",
                argv[0],
                argv.len() - 1
            ));
            return Err(TclError::Arity);
        }

        let mut status;

        status = interp.eval(&ppd.body)?;

        // Clean up call frame
        // TODO: This needs to be done under all circumstances.
        interp.callframes.pop();

        if status == Status::Return {
            status = Status::Ok;
        }

        Ok(status)
    }

    fn cmd_if(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 3, 5)?;

        let cond = &argv[1];
        let thenb = &argv[2];
        let elseb: Option<&String> = if argv.len() == 5 {
            Some(&argv[4])
        } else {
            None
        };

        let res = interp.eval(cond);

        if res.is_err() || res.unwrap() != Status::Ok {
            return res;
        }

        match interp.result.as_ref().unwrap().parse::<i64>() {
            Ok(0) => match elseb {
                Some(elseb) => interp.eval(elseb),
                None => Ok(Status::Ok),
            },
            Ok(_) => interp.eval(thenb),
            Err(_) => {
                interp.result = Some(format!("invalid number: '{cond}'"));
                return Err(TclError::InvalidNumber);
            }
        }
    }

    fn cmd_proc(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 4, 4)?;

        let ppd = Rc::new(ProcPrivdata {
            args: argv[2].clone(),
            body: argv[3].clone(),
        });

        interp.register_command(&argv[1], call_proc, Some(ppd))?;

        Ok(Status::Ok)
    }

    fn cmd_while(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 3, 3)?;

        let cond = &argv[1];
        let body = &argv[2];

        loop {
            let res = interp.eval(cond)?;

            if res != Status::Ok {
                return Ok(res);
            }

            let val = interp.result.as_ref().unwrap().parse::<i64>();

            if val.is_err() {
                interp.result = Some(format!("invalid number: '{cond}'"));
                return Err(TclError::InvalidNumber);
            }

            if val.unwrap() == 0 {
                return Ok(Status::Ok);
            }

            let res2 = interp.eval(body)?;

            if res2 == Status::Continue || res2 == Status::Ok {
                continue;
            } else if res2 == Status::Break {
                break;
            } else {
                return Ok(res2);
            }
        }
        Ok(Status::Ok)
    }

    fn cmd_continue(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 1, 1)?;
        Ok(Status::Continue)
    }

    fn cmd_break(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 1, 1)?;
        Ok(Status::Break)
    }

    fn cmd_return(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
    ) -> Result<Status, TclError> {
        check_arity(interp, argv, 2, 2)?;
        interp.result = Some(argv[1].clone());
        Ok(Status::Return)
    }

    fn cmd_math(
        interp: &mut Interp,
        argv: &[String],
        _privdata: Option<Rc<dyn Any>>,
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
            ">" => interp.result = Some(format!("{}", (a > b) as i64)),
            "<" => interp.result = Some(format!("{}", (a < b) as i64)),
            "==" => interp.result = Some(format!("{}", (a == b) as i64)),
            "!=" => interp.result = Some(format!("{}", (a != b) as i64)),
            ">=" => interp.result = Some(format!("{}", (a >= b) as i64)),
            "<=" => interp.result = Some(format!("{}", (a <= b) as i64)),
            _ => {
                interp.result = Some(format!("unknown operator: '{}'", argv[1]));
                return Err(TclError::General);
            }
        }

        Ok(Status::Ok)
    }

    impl Interp {
        pub fn new() -> Interp {
            let mut interp = Interp {
                commands: Vec::new(),
                callframes: Vec::new(),
                result: None,
                trace_parser: false,
            };
            interp.callframes.push(CallFrame::new());
            interp
        }

        pub fn set_var(&mut self, name: &str, value: &str) -> Result<Status, TclError> {
            let callframe = self.callframes.last_mut().unwrap();
            callframe.set_var(name, value)?;
            Ok(Status::Ok)
        }

        fn get_var(&self, name: &str) -> Option<&Var> {
            let callframe = self.callframes.last().unwrap();
            callframe.vars.iter().find(|v| v.name == name)
        }

        pub fn get_command(&self, name: &str) -> Option<&Cmd> {
            self.commands.iter().find(|c| c.name == name)
        }

        pub fn register_command(
            &mut self,
            name: &str,
            cmd: CmdFunc,
            privdata: Option<Rc<dyn Any>>,
        ) -> Result<Status, TclError> {
            if self.get_command(name).is_some() {
                self.result = Some(format!("command already defined: '{name}'"));
                return Err(TclError::CommandAlreadyDefined);
            }

            let cmd = Cmd {
                name: name.to_string(),
                cmd_func: cmd,
                privdata,
            };

            self.commands.push(cmd);

            Ok(Status::Ok)
        }

        pub fn register_core_commands(&mut self) {
            // Basics
            let _ = self.register_command("puts", cmd_puts, None);
            let _ = self.register_command("set", cmd_set, None);

            // Procs and flow control
            let _ = self.register_command("proc", cmd_proc, None);
            let _ = self.register_command("return", cmd_return, None);
            let _ = self.register_command("if", cmd_if, None);
            let _ = self.register_command("continue", cmd_continue, None);
            let _ = self.register_command("break", cmd_break, None);
            let _ = self.register_command("while", cmd_while, None);

            // Math
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
            p.trace = self.trace_parser;

            let mut argv: Vec<String> = Vec::new();
            loop {
                let prevtype = p.token;
                let token = p.next();
                let mut t = p.token_body();

                if token == Token::Eof {
                    break;
                } else if token == Token::Var {
                    let var = self.get_var(t);
                    if var.is_some() {
                        t = &var.unwrap().value;
                    } else {
                        self.result = Some(format!("variable not found: '{t}'"));
                        return Err(TclError::VariableNotFound);
                    }
                } else if token == Token::Cmd {
                    let ret = self.eval(t);
                    if ret.is_err() || ret.unwrap() != Status::Ok {
                        return ret;
                    }
                    t = self.result.as_ref().unwrap();
                } else if token == Token::Sep {
                    continue;
                } else if token == Token::Eol {
                    if !argv.is_empty() {
                        let cmd_name = &argv[0];
                        let cmd = self.get_command(cmd_name);
                        if let Some(cmd) = cmd {
                            let privdata_clone = cmd.privdata.as_ref().map(Rc::clone);
                            let res = (cmd.cmd_func)(self, &argv, privdata_clone);
                            if (res.is_ok() && res.ok().unwrap() != Status::Ok) || res.is_err() {
                                return res;
                            }
                        } else {
                            self.result = Some(format!("command not found: '{cmd_name}'"));
                            return Err(TclError::CommandNotFound);
                        }
                    }
                    argv.clear();

                    continue;
                }

                if prevtype == Token::Sep || prevtype == Token::Eol {
                    // dup string
                    let duped = t.to_string();
                    argv.push(duped);
                } else {
                    // append to prev token
                    let prev = argv.last().unwrap();
                    let new = format!("{prev}{t}");
                    argv.pop();
                    argv.push(new);
                }
            }
            Ok(Status::Ok)
        }
    }
}
