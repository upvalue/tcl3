mod tcl;
use std::process;
use tcl::tcl::*;

fn parse_args(args: &[String]) -> (bool, String) {
    let mut trace_enabled = false;
    let mut filename = None;

    let mut i = 1; // Skip program name
    while i < args.len() {
        match args[i].as_str() {
            "--trace-parser" => {
                trace_enabled = true;
            }
            arg if !arg.starts_with("--") => {
                if filename.is_none() {
                    filename = Some(arg.to_string());
                } else {
                    print_usage(&args[0]);
                    process::exit(1);
                }
            }
            _ => {
                eprintln!("Unknown option: {}", args[i]);
                print_usage(&args[0]);
                process::exit(1);
            }
        }
        i += 1;
    }

    let filename = filename.unwrap_or_else(|| {
        print_usage(&args[0]);
        process::exit(1);
    });

    (trace_enabled, filename)
}

fn print_usage(program_name: &str) {
    eprintln!("Usage: {} [--trace-parser] <filename>", program_name);
}

fn main() {
    let args: Vec<String> = std::env::args().collect();

    let (trace_enabled, filename) = parse_args(&args);

    let contents = std::fs::read_to_string(&filename).unwrap_or_else(|e| {
        eprintln!("Error reading file: {}", e);
        std::process::exit(1);
    });

    /*let mut p = Parser::new(&contents);

    p.set_trace(trace_enabled);

    while !p.done() {
        p.next();
    }

    // One final EOF
    p.next();*/

    let mut i = Interp::new();
    i.register_core_commands();

    let res = i.eval(&contents);

    if res.is_ok() {
        println!("Result: {:?}", res.ok().unwrap());
    } else {
        eprintln!("Error: {:?}", res.err().unwrap());
    }
}
