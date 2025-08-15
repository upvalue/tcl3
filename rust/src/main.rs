mod tcl;
use tcl::tcl::*;
fn main() {
    let mut p = Parser::new(&"set a 4");

    while let tk = p.next() {
        if tk == Token::EOF {
            break;
        }
    }
}
