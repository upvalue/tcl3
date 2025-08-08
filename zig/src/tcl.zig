const std = @import("std");
const print = std.debug.print;

const stderr = std.io.getStdErr().writer();
const stdio = std.io.getStdOut().writer();

// The Zig version

// There aren't many explanatory comments in this one because it's structured
// the same as the C++ version and most code is fairly similar.

// Here's a couple things that might be interesting to poke at though:

// - Zig has nicer features for unwinding the stack, but now there's a disjoint
// error/status union that is returned from a lot of things. Maybe there's a better
// way to combine those so you're not combining try and then if branches?

// - Memory management code feels a little janky, maybe it can be simplified

pub const TclError = error{
    General,
    Arity,
    CommandNotFound,
    CommandAlreadyDefined,
    VariableNotFound,
};

pub const Error = TclError || std.mem.Allocator.Error || std.fmt.AllocPrintError || std.fmt.ParseIntError;

pub const Status = enum {
    OK,
    // We remove the ERR code in Zig because error unions
    // are more ergonomic.
    RETURN,
    BREAK,
    CONTINUE,
};

pub const Token = enum {
    ESC,
    STR,
    CMD,
    VAR,
    SEP,
    EOL,
    // We can do away with this in Zig because we can instead use
    // optional values to know when to terminate. However it's kept
    // to make output easy to compare with the C++ version.
    EOF,
};

pub const Parser = struct {
    body: []u8,
    trace: bool = false,
    cursor: usize = 0,
    begin: usize = 0,
    end: usize = 0,

    in_string: bool = false,
    in_brace: bool = false,
    in_quote: bool = false,
    in_command: bool = false,

    brace_level: usize = 0,
    token: Token = Token.EOL,
    terminating_char: u8 = 0,

    pub fn done(p: *Parser) bool {
        return p.cursor >= p.body.len;
    }

    pub fn peek(p: *Parser) u8 {
        return p.body[p.cursor];
    }

    pub fn getc(p: *Parser) u8 {
        const c = p.body[p.cursor];
        p.cursor += 1;
        return c;
    }

    pub fn back(p: *Parser) void {
        p.cursor -= 1;
    }

    pub fn token_body(p: *Parser) []u8 {
        return p.body[p.begin..p.end];
    }

    pub fn consume_whitespace(p: *Parser) void {
        while (!p.done()) {
            const c = p.peek();
            if (c == ' ' or c == '\n' or c == '\r' or c == '\t' or c == ';') {
                _ = p.getc();
            } else {
                return;
            }
        }
    }

    pub fn recurse(p: *Parser, sub: *Parser, terminating_char: u8) void {
        sub.terminating_char = terminating_char;
        while (sub.next() != Token.EOF) {}
        p.cursor = p.cursor + sub.cursor;
    }

    fn _next(p: *Parser) Token {
        if (p.done()) {
            if (p.token != Token.EOF and p.token != Token.EOL) {
                p.token = Token.EOL;
            } else {
                p.token = Token.EOF;
            }
            return p.token;
        }

        p.token = Token.ESC;
        p.begin = p.cursor;

        var c: u8 = 0;
        var adj: usize = 0;

        while (!p.done()) {
            adj = 0;

            c = p.getc();

            if (p.terminating_char == c) {
                return Token.EOF;
            }

            switch (c) {
                '{' => {
                    if (p.in_quote or p.in_string) continue;

                    if (!p.in_brace) {
                        p.begin += 1;
                        p.token = Token.STR;
                        p.in_brace = true;
                    }

                    p.brace_level += 1;
                },
                '}' => {
                    if (p.in_quote or p.in_string) continue;

                    if (p.brace_level > 0) {
                        p.brace_level -= 1;
                        if (p.brace_level == 0) {
                            p.in_brace = false;
                            adj = 1;
                            break;
                        }
                    }
                },
                '[' => {
                    if (p.in_string or p.in_quote or p.in_brace) continue;

                    var sub: Parser = .{
                        .body = p.body[p.cursor..],
                    };

                    p.begin += 1;
                    p.recurse(&sub, ']');
                    adj = 1;
                    p.token = Token.CMD;
                    break;
                },
                '$' => {
                    if (p.in_string or p.in_brace) continue;

                    if (p.in_quote and p.cursor != p.begin + 1) {
                        p.back();
                        break;
                    }

                    p.begin += 1;
                    p.token = Token.VAR;
                    p.in_string = true;
                },
                '#' => {
                    if (p.in_string or p.in_quote or p.in_brace) continue;

                    while (!p.done()) {
                        if (p.getc() == '\n') {
                            break;
                        }
                    }

                    return @call(.always_tail, Parser._next, .{p});
                },
                '"' => {
                    if (p.in_quote) {
                        p.in_quote = false;
                        adj = 1;
                        break;
                    }

                    p.in_quote = true;
                    p.begin += 1;
                    adj = 1;
                },
                '\n', '\r', '\t', ';', ' ' => {
                    if (p.in_brace) {
                        continue;
                    }

                    if (p.in_string) {
                        p.back();
                        p.in_string = false;
                        break;
                    }

                    if (p.in_quote) {
                        continue;
                    }

                    p.token = if (c == '\n' or c == ';') Token.EOL else Token.SEP;
                    p.consume_whitespace();
                    break;
                },
                else => {
                    if (!p.in_brace and !p.in_quote) {
                        p.in_string = true;
                    }
                },
            }
        }

        p.end = p.cursor - adj;

        return p.token;
    }

    pub fn next(p: *Parser) Token {
        const t = _next(p);
        if (p.trace) {
            std.debug.print("{{\"type\": \"TK_{s}\", \"begin\": {}, \"end\": {}, \"body\": \"{}\"}}\n", .{ @tagName(t), p.begin, p.end, std.zig.fmtEscapes(p.body[p.begin..p.end]) });
        }
        return t;
    }
};

pub const Privdata = struct {
    data: *anyopaque,
    finalizer: *const fn (std.mem.Allocator, *anyopaque) void,
};

pub const ProcPrivdata = struct {
    args: []u8,
    body: []u8,
};

pub const CmdFunc = fn (*Interp, std.ArrayList([]u8), ?*Privdata) Error!Status;

pub const Cmd = struct {
    name: []u8,
    cmd_func: *const CmdFunc,
    string: ?*[]u8 = null,
    privdata: ?*Privdata = null,

    pub fn deinit(self: *const Cmd, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        if (self.privdata) |pd| {
            pd.finalizer(allocator, pd.data);
            allocator.destroy(pd);
        }
    }
};

pub const Var = struct {
    name: []u8,
    value: []u8,

    pub fn init(allocator: std.mem.Allocator, name: []u8, value: []u8) !Var {
        return Var{
            .name = try allocator.dupe(u8, name),
            .value = try allocator.dupe(u8, value),
        };
    }

    pub fn deinit(self: *const Var, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        allocator.free(self.value);
    }

    pub fn set_value(self: *Var, allocator: std.mem.Allocator, value: []u8) !void {
        allocator.free(self.value);
        self.value = try allocator.dupe(u8, value);
    }
};

pub const CallFrame = struct {
    vars: std.ArrayList(Var),

    pub fn init(allocator: std.mem.Allocator) CallFrame {
        return CallFrame{
            .vars = std.ArrayList(Var).init(allocator),
        };
    }

    pub fn deinit(self: *const CallFrame, allocator: std.mem.Allocator) void {
        for (self.vars.items) |v| v.deinit(allocator);
        self.vars.deinit();
    }
};

fn check_arity(interp: *Interp, name: []const u8, argv: std.ArrayList([]u8), min: usize, max: usize) Error!Status {
    if (argv.items.len < min or argv.items.len > max) {
        try interp.set_result_fmt("wrong number of arguments to {s}: expected {d}-{d}, got {d}", .{ name, min, max, argv.items.len });
        return error.Arity;
    }
    return Status.OK;
}

fn cmd_set(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "set", argv, 3, 3);

    const name = argv.items[1];
    const value = argv.items[2];
    try i.set_var(name, value);

    return Status.OK;
}

fn cmd_puts(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "puts", argv, 2, 2);

    stdio.print("{s}\n", .{argv.items[1]}) catch return error.General;
    return Status.OK;
}

fn cmd_continue(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "continue", argv, 1, 1);
    return Status.CONTINUE;
}

fn cmd_break(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "break", argv, 1, 1);
    return Status.BREAK;
}

fn cmd_while(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "while", argv, 3, 3);

    const cond = argv.items[1];
    const body = argv.items[2];

    while (true) {
        var res = try i.eval(cond);

        if (res != Status.OK) {
            return res;
        }

        const val = try std.fmt.parseInt(i64, i.result.?, 10);

        if (val == 0) {
            return Status.OK;
        }

        res = try i.eval(body);

        if (res == Status.CONTINUE or res == Status.OK) {
            continue;
        } else if (res == Status.BREAK) {
            break;
        } else {
            return res;
        }
    }

    return Status.OK;
}

fn cmd_return(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "return", argv, 2, 2);
    try i.set_result(argv.items[1]);
    return Status.RETURN;
}

fn cmd_proc(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "proc", argv, 4, 4);

    const alist = try i.allocator.dupe(u8, argv.items[2]);
    const body = try i.allocator.dupe(u8, argv.items[3]);

    // Allocate proc on heap
    const ppd = try i.allocator.create(ProcPrivdata);
    ppd.* = ProcPrivdata{ .args = alist, .body = body };
    const pd = try i.allocator.create(Privdata);
    pd.* = Privdata{ .data = ppd, .finalizer = proc_finalizer };

    _ = try i.register_command(argv.items[1], call_proc, pd);

    return Status.OK;
}

fn cmd_if(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "if", argv, 3, 5);

    const cond = argv.items[1];
    const thenb = argv.items[2];
    const elseb = if (argv.items.len == 5) argv.items[4] else null;

    const res = try i.eval(cond);

    if (res != Status.OK) {
        return res;
    }

    const val = try std.fmt.parseInt(i64, i.result.?, 10);

    if (val != 0) {
        return i.eval(thenb);
    } else if (elseb != null) {
        return i.eval(elseb.?);
    }
    return Status.OK;
}

fn cmd_math(i: *Interp, argv: std.ArrayList([]u8), _: ?*Privdata) Error!Status {
    _ = try check_arity(i, "math", argv, 3, 3);

    const a = try std.fmt.parseInt(i64, argv.items[1], 10);

    const b = try std.fmt.parseInt(i64, argv.items[2], 10);

    const cmd = argv.items[0];

    var c: i64 = 0;

    if (cmd[0] == '+') {
        c = a + b;
    } else if (cmd[0] == '-') {
        c = a - b;
    } else if (cmd[0] == '*') {
        c = a * b;
    } else if (cmd[0] == '/') {
        c = @divFloor(a, b);
    } else if (cmd[0] == '>') {
        c = @intFromBool(if (cmd.len > 1) a >= b else a > b);
    } else if (cmd[0] == '<') {
        c = @intFromBool(if (cmd.len > 1) a <= b else a < b);
    } else if (cmd[0] == '=' and cmd[1] == '=') {
        c = @intFromBool(a == b);
    } else if (cmd[0] == '!' and cmd[1] == '=') {
        c = @intFromBool(a != b);
    }

    try i.set_result_fmt("{d}", .{c});

    return Status.OK;
}

fn proc_finalizer(allocator: std.mem.Allocator, data: *anyopaque) void {
    const pdp: *ProcPrivdata = @alignCast(@ptrCast(data));
    allocator.free(pdp.args);
    allocator.free(pdp.body);
    allocator.destroy(pdp);
}

fn call_proc(i: *Interp, argv: std.ArrayList([]u8), privdata: ?*Privdata) Error!Status {
    const ppd: *ProcPrivdata = @alignCast(@ptrCast(privdata.?.data));
    const cf = CallFrame.init(i.allocator);
    try i.callframes.append(cf);
    defer i.drop_call_frame();

    var arity: u64 = 0;
    const alist = ppd.args;
    const body = ppd.body;

    var j: usize = 0;
    var start: usize = 0;

    while (j < alist.len) {
        while (j < alist.len and alist[j] == ' ') {
            j += 1;
        }

        start = j;

        while (j < alist.len and alist[j] != ' ') {
            j += 1;
        }

        try i.set_var(alist[start..j], argv.items[arity + 1]);

        arity += 1;

        if (j >= alist.len) {
            break;
        }
    }

    var status = Status.OK;

    if (arity != argv.items.len - 1) {
        try i.set_result_fmt("wrong number of arguments for {s}: expected {d}, got {d}", .{ argv.items[0], arity, argv.items.len - 1 });
        return error.Arity;
    }

    status = try i.eval(body);

    if (status == Status.RETURN) {
        status = Status.OK;
    }

    return status;
}

pub const Interp = struct {
    commands: std.ArrayList(Cmd),
    callframes: std.ArrayList(CallFrame),
    allocator: std.mem.Allocator,
    trace_parser: bool,
    result: ?[]u8 = null,

    pub fn init(allocator: std.mem.Allocator) !Interp {
        const result = try allocator.alloc(u8, 0);
        var i = Interp{
            .commands = std.ArrayList(Cmd).init(allocator),
            .callframes = std.ArrayList(CallFrame).init(allocator),
            .allocator = allocator,
            .trace_parser = false,
            .result = result,
        };

        const cf = CallFrame.init(allocator);
        try i.callframes.append(cf);

        try i.register_core_commands();

        return i;
    }

    pub fn deinit(self: *Interp) void {
        if (self.result) |r| {
            self.allocator.free(r);
        }

        for (self.commands.items) |c| c.deinit(self.allocator);
        self.commands.deinit();

        for (self.callframes.items) |cf| cf.deinit(self.allocator);
        self.callframes.deinit();

        self.* = undefined;
    }

    pub fn set_result(self: *Interp, result: []u8) !void {
        if (self.result) |r| {
            self.allocator.free(r);
        }
        self.result = try self.allocator.dupe(u8, result);
    }

    pub fn set_result_fmt(self: *Interp, comptime format: []const u8, args: anytype) !void {
        const result = try std.fmt.allocPrint(self.allocator, format, args);
        if (self.result) |r| {
            self.allocator.free(r);
        }
        self.result = result;
    }

    ///// CALL FRAMES AND VARIABLES
    pub fn get_var(self: *Interp, name: []const u8) ?*Var {
        const cf = &self.callframes.items[self.callframes.items.len - 1];
        for (cf.vars.items, 0..) |v, i| {
            if (std.mem.eql(u8, v.name, name)) {
                return &cf.vars.items[i];
            }
        }
        return null;
    }

    pub fn set_var(self: *Interp, name: []u8, value: []u8) !void {
        const cf = &self.callframes.items[self.callframes.items.len - 1];
        if (self.get_var(name)) |v| {
            try v.set_value(self.allocator, value);
        } else {
            const variable = try Var.init(self.allocator, name, value);
            try cf.vars.append(variable);
        }
    }

    pub fn drop_call_frame(self: *Interp) void {
        const cf = self.callframes.pop();
        cf.?.deinit(self.allocator);
    }

    ///// COMMANDS

    pub fn get_command(self: *Interp, cmd: []const u8) ?*Cmd {
        var idx: usize = 0;
        while (idx < self.commands.items.len) : (idx += 1) {
            if (std.mem.eql(u8, self.commands.items[idx].name, cmd)) {
                return &self.commands.items[idx];
            }
        }
        return null;
    }

    pub fn register_command(self: *Interp, name: []const u8, cmd_func: *const CmdFunc, privdata: ?*Privdata) Error!Status {
        if (self.get_command(name)) |_| {
            try self.set_result_fmt("command already defined: '{s}'", .{name});
            return error.CommandAlreadyDefined;
        }
        const duped_name = try self.allocator.dupe(u8, name);
        try self.commands.append(.{ .name = duped_name, .cmd_func = cmd_func, .privdata = privdata });
        return Status.OK;
    }

    pub fn register_core_commands(self: *Interp) !void {
        _ = try self.register_command("set", cmd_set, null);
        _ = try self.register_command("puts", cmd_puts, null);
        _ = try self.register_command("proc", cmd_proc, null);
        _ = try self.register_command("return", cmd_return, null);

        // Branching and flow control
        _ = try self.register_command("if", cmd_if, null);
        _ = try self.register_command("while", cmd_while, null);
        _ = try self.register_command("break", cmd_break, null);
        _ = try self.register_command("continue", cmd_continue, null);

        // Math commands
        _ = try self.register_command("+", cmd_math, null);
        _ = try self.register_command("-", cmd_math, null);
        _ = try self.register_command("*", cmd_math, null);
        _ = try self.register_command("/", cmd_math, null);
        _ = try self.register_command(">", cmd_math, null);
        _ = try self.register_command("<", cmd_math, null);
        _ = try self.register_command("==", cmd_math, null);
        _ = try self.register_command("!=", cmd_math, null);
        _ = try self.register_command(">=", cmd_math, null);
        _ = try self.register_command("<=", cmd_math, null);
    }

    pub fn eval(self: *Interp, str: []u8) Error!Status {
        var argv = std.ArrayList([]u8).init(self.allocator);
        defer argv.deinit();
        defer for (argv.items) |a| {
            self.allocator.free(a);
        };

        var p = Parser{
            .body = str,
            .trace = self.trace_parser,
        };

        while (true) {
            var prevtype = p.token;
            defer prevtype = p.token;

            const token = p.next();
            var t = p.token_body();

            if (token == Token.EOF) {
                break;
            } else if (token == Token.VAR) {
                const variable = self.get_var(t);
                if (variable) |v| {
                    t = v.value;
                } else {
                    try self.set_result_fmt("variable not found: '{s}'", .{t});
                    return error.VariableNotFound;
                }
            } else if (token == Token.CMD) {
                const ret = try self.eval(t);
                if (ret != Status.OK) {
                    return ret;
                }
                t = self.result.?;
            } else if (token == Token.SEP) {
                continue;
            } else if (token == Token.EOL) {
                // Command

                if (argv.items.len > 0) {
                    if (self.get_command(argv.items[0])) |c| {
                        const res = try c.cmd_func(self, argv, c.privdata);
                        if (res != Status.OK) {
                            return res;
                        }
                    } else {
                        try self.set_result_fmt("command not found: '{s}'", .{argv.items[0]});
                        return error.CommandNotFound;
                    }
                }

                for (argv.items) |a| {
                    self.allocator.free(a);
                }
                argv.clearRetainingCapacity();

                continue;
            }

            if (prevtype == Token.SEP or prevtype == Token.EOL) {
                // dup string
                const duped = try self.allocator.dupe(u8, t);
                try argv.append(duped);
            } else {
                // append to prev token
                const prev = argv.items[argv.items.len - 1];
                const new = try std.fmt.allocPrint(self.allocator, "{s}{s}", .{ prev, t });
                self.allocator.free(prev);
                argv.items[argv.items.len - 1] = new;
            }
        }

        return Status.OK;
    }
};
