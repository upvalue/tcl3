mod tcl;
use tcl::tcl::*;
fn main() {
    let mut p = Parser::new(String::from("test"));

    while let tk = p.next() {
        if tk == Token::EOF {
            break;
        }
    }
}
