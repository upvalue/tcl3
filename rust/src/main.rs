mod tcl;
use clap::Parser;
use std::ffi::{CStr, CString, c_char};
use tcl::tcl::*;

unsafe extern "C" {
    fn linenoise(prompt: *const c_char) -> *mut c_char;
    fn linenoiseFree(ptr: *mut c_char);
}

#[derive(Parser, Debug)]
struct Args {
    /// If true, print parser tokens to stderr
    #[arg(short, long, default_value_t = false)]
    trace_parser: bool,

    /// If true, open a read eval print loop
    #[arg(short, long, default_value_t = false)]
    repl: bool,

    /// Files to evaluate
    #[arg(
        value_name = "FILES",
        num_args = 1..,
    )]
    files: Vec<String>,
}

fn main() {
    let args = Args::parse();

    let mut i = Interp::new();
    i.register_core_commands();

    i.trace_parser = args.trace_parser;

    for file in args.files {
        let contents = std::fs::read_to_string(&file).unwrap_or_else(|e| {
            eprintln!("Error reading file: {}", e);
            std::process::exit(1);
        });

        let res = i.eval(&contents);

        if res.is_err() {
            eprintln!("Error: {:?} {:?}", res.err().unwrap(), i.result);

            std::process::exit(1);
        }
    }

    if args.repl {
        let prompt = CString::new("> ").unwrap();
        loop {
            let ptr = unsafe {
                let ptr = linenoise(prompt.as_ptr());
                if ptr.is_null() {
                    break;
                }
                ptr
            };

            let cline = unsafe { CStr::from_ptr(ptr) };

            let line = cline.to_string_lossy().into_owned();

            let res = i.eval(line.as_str());

            if res.is_ok() {
                println!("{:?}", res.ok().unwrap());
            } else {
                eprintln!("Error: {:?} {:?}", res.err().unwrap(), i.result);
            }

            unsafe {
                linenoiseFree(ptr);
            }
        }
    }
}
