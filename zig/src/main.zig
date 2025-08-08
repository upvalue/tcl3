const std = @import("std");
const tcl = @import("tcl.zig");

const clap = @import("clap");

const help =
    \\ -h, --help Display help
    \\ -t, --parser-trace Trace parser
    \\ -p, --parser-only Only parse, don't evaluate
    \\ -r, --repl Open REPL
    \\ --allocator-arena Use ArenaAllocator
    \\ --allocator-libc Use libc allocator
    \\<str>...
    \\
;

pub fn exec(i: *tcl.Interp, str: []u8, parser_only: bool) !void {
    if (parser_only) {
        var p = tcl.Parser{
            .body = str,
            .trace = i.trace_parser,
        };

        while (p.next() != tcl.Token.EOF) {}
    } else {
        _ = i.eval(str) catch {
            std.debug.print("error: {?s}\n", .{i.result});
        };
    }
}

pub fn main() !void {
    var gpaalloc = std.heap.GeneralPurposeAllocator(.{}){};

    defer {
        const deinit_status = gpaalloc.deinit();
        if (deinit_status == .leak) {
            std.debug.print("Memory leak detected\n", .{});
        }
    }

    const params = comptime clap.parseParamsComptime(help);

    var diag = clap.Diagnostic{};
    var res = clap.parse(clap.Help, &params, clap.parsers.default, .{
        .diagnostic = &diag,
        .allocator = gpaalloc.allocator(),
    }) catch |err| {
        try diag.report(std.io.getStdErr().writer(), err);
        return err;
    };
    defer res.deinit();

    const repl = false;
    const parser_trace = res.args.@"parser-trace" != 0;
    const parser_only = res.args.@"parser-only" != 0;
    var alloc = gpaalloc.allocator();
    var arenaAlloc: ?std.heap.ArenaAllocator = null;

    // Print help
    if (res.args.help != 0) {
        std.debug.print(
            \\ Usage: tcl <flags> [file]...
            \\{s}
            \\If allocator is not specified, GeneralPurposeAllocator is used.
        , .{help});
        return;
    }

    // Select allocator for interpreter
    if (res.args.@"allocator-arena" != 0) {
        arenaAlloc = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        alloc = arenaAlloc.?.allocator();
    } else if (res.args.@"allocator-libc" != 0) {
        alloc = std.heap.c_allocator;
    }

    var i: tcl.Interp = try tcl.Interp.init(alloc);
    defer {
        i.deinit();
    }

    i.trace_parser = parser_trace;

    // Operate on files
    for (res.positionals[0]) |arg| {
        const file = std.fs.cwd().openFile(arg, .{}) catch |err| {
            std.debug.print("Error: {}\n", .{err});
            return err;
        };
        defer file.close();

        const content = file.readToEndAlloc(alloc, 1024 * 1024) catch |err| {
            std.debug.print("Error: {}\n", .{err});
            return err;
        };
        defer alloc.free(content);

        try exec(&i, content, parser_only);
    }

    if (repl) {}

    if (arenaAlloc) |a| {
        a.deinit();
    }
}
