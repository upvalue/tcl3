const std = @import("std");
const print = std.debug.print;

const Status = enum {
    S_OK,
    S_ERR,
    S_RETURN,
    S_BREAK,
    S_CONTINUE,
};

const Token = enum {
    TK_ESC,
    TK_STR,
    TK_CMD,
    TK_VAR,
    TK_SEP,
    TK_EOL,
    // We can do away with this in Zig because we can instead use
    // optional values to know when to terminate. However it's kept
    // to make output easy to compare with the C++ version.
    TK_EOF,
};

const Parser = struct {
    body: []const u8,
    trace: bool = false,
    cursor: usize = 0,
    begin: usize = 0,
    end: usize = 0,

    in_string: bool = false,
    in_brace: bool = false,
    in_quote: bool = false,
    in_command: bool = false,

    brace_level: usize = 0,
    token: Token = Token.TK_EOF,
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

    fn _next(p: *Parser) Token {
        if (p.done()) {
            if (p.token != Token.TK_EOF and p.token != Token.TK_EOL) {
                p.token = Token.TK_EOL;
            } else {
                p.token = Token.TK_EOF;
            }
            return p.token;
        }

        p.token = Token.TK_ESC;
        p.begin = p.cursor;

        var c: u8 = 0;

        while (!p.done()) {
            c = p.getc();
            // print("char: {c}, cursor: {d}\n", .{ c, p.cursor });
            switch (c) {
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

                    p.token = if (c == '\n' or c == ';') Token.TK_EOL else Token.TK_SEP;
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

        p.end = p.cursor;

        return p.token;
    }

    pub fn next(p: *Parser) ?Token {
        const t = _next(p);
        if (p.trace) {
            std.debug.print("{{\"type\": \"{s}\", \"begin\": {}, \"end\": {}, \"body\": \"{}\"}}\n", .{ @tagName(t), p.begin, p.end, std.zig.fmtEscapes(p.body[p.begin..p.end]) });
        }
        return t;
    }
};

pub fn main() !void {
    var p = Parser{
        .body = "test\n",
        .trace = true,
    };

    while (p.next()) |token| {
        if (token == Token.TK_EOF) {
            break;
        }
    }
}
