const std = @import("std");
const print = std.debug.print;

pub const Error = error{
    General,
    Arity,
    CommandNotFound,
    CommandAlreadyDefined,
};

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
            c = p.getc();

            if (p.terminating_char == c) {
                return Token.EOF;
            }

            switch (c) {
                '{' => {
                    if (p.in_quote or p.in_string) {
                        continue;
                    }

                    if (!p.in_brace) {
                        p.begin += 1;
                        p.token = Token.STR;
                        p.in_brace = true;
                    }

                    p.brace_level += 1;
                },
                '}' => {
                    if (p.in_quote or p.in_string) {
                        continue;
                    }

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
                    if (p.in_string or p.in_quote or p.in_brace) {
                        continue;
                    }

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
                    if (p.in_string or p.in_brace) {
                        continue;
                    }

                    if (p.in_quote and p.cursor != p.begin + 1) {
                        p.back();
                        break;
                    }

                    p.begin += 1;
                    p.token = Token.VAR;
                    p.in_string = true;
                },
                '#' => {
                    if (p.in_string or p.in_quote or p.in_brace) {
                        continue;
                    }

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

pub const CmdFunc = fn (*Interp, std.ArrayList([]u8), ?*anyopaque) Error!Status;

pub const Cmd = struct {
    name: []u8,
    cmd_func: *const CmdFunc,
    string: ?*[]u8 = null,
    privdata: ?*anyopaque = null,

    pub fn deinit(self: *const Cmd, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
    }
};

fn check_arity(interp: *Interp, name: []const u8, argv: std.ArrayList([]u8), min: usize, max: usize) Error!Status {
    if (argv.items.len < min or argv.items.len > max) {
        interp.set_result_fmt("wrong number of arguments to {s}: expected {d}-{d}, got {d}", .{ name, min, max, argv.items.len }) catch {
            return error.General;
        };
        return error.Arity;
    }
    return Status.OK;
}

fn puts(i: *Interp, argv: std.ArrayList([]u8), _: ?*anyopaque) Error!Status {
    _ = try check_arity(i, "puts", argv, 2, 2);

    std.debug.print("{s}\n", .{argv.items[1]});
    return Status.OK;
}

pub const Interp = struct {
    commands: std.ArrayList(Cmd),
    allocator: std.mem.Allocator,
    trace_parser: bool,
    result: ?[]u8 = null,

    pub fn init(allocator: std.mem.Allocator) Interp {
        const result = allocator.alloc(u8, 0) catch unreachable;
        return Interp{
            .commands = std.ArrayList(Cmd).init(allocator),
            .allocator = allocator,
            .trace_parser = false,
            .result = result,
        };
    }

    pub fn deinit(self: *Interp) void {
        if (self.result) |r| {
            std.debug.print("freeing result: {s}\n", .{r});
            self.allocator.free(r);
        }

        for (self.commands.items) |c| c.deinit(self.allocator);
        self.commands.deinit();

        self.* = undefined;
    }

    pub fn set_result(self: *Interp, result: []u8) !void {
        if (self.result) |r| {
            self.allocator.free(r);
        }
        self.result = try self.allocator.dupe(u8, result);
    }

    pub fn set_result_fmt(self: *Interp, comptime format: []const u8, args: anytype) !void {
        const result = std.fmt.allocPrint(self.allocator, format, args) catch unreachable;
        if (self.result) |r| {
            self.allocator.free(r);
        }
        self.result = result;
    }

    pub fn get_command(self: *Interp, cmd: []const u8) ?*Cmd {
        var idx: usize = 0;
        while (idx < self.commands.items.len) : (idx += 1) {
            if (std.mem.eql(u8, self.commands.items[idx].name, cmd)) {
                return &self.commands.items[idx];
            }
        }
        return null;
    }

    pub fn register_command(self: *Interp, name: []const u8, cmd_func: *const CmdFunc, privdata: ?*anyopaque) Error!Status {
        if (self.get_command(name)) |_| {
            self.set_result_fmt("command already defined: '{s}'", .{name}) catch {};
            return error.CommandAlreadyDefined;
        }
        const duped_name = self.allocator.dupe(u8, name) catch return error.General;
        self.commands.append(.{ .name = duped_name, .cmd_func = cmd_func, .privdata = privdata }) catch return error.General;
        return Status.OK;
    }

    pub fn register_core_commands(self: *Interp) !void {
        _ = try self.register_command("puts", puts, null);
    }

    pub fn eval(self: *Interp, str: []u8) !Status {
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
            const token = p.next();
            const t = p.token_body();

            if (token == Token.EOF) {
                break;
            } else if (token == Token.SEP) {
                prevtype = p.token;
                continue;
            } else if (token == Token.EOL) {
                // Command
                prevtype = p.token;

                if (argv.items.len > 0) {
                    if (self.get_command(argv.items[0])) |c| {
                        _ = try c.cmd_func(self, argv, c.privdata);
                    } else {
                        try self.set_result_fmt("command not found: '{s}'", .{argv.items[0]});
                        return error.CommandNotFound;
                    }
                }
            }

            if (prevtype == Token.SEP or prevtype == Token.EOL) {
                // dup string
                const duped = self.allocator.dupe(u8, t) catch return error.General;
                try argv.append(duped);
            } else {
                // append to prev token
            }

            prevtype = token;
        }

        return Status.OK;
    }
};
