package main

import testing "core:testing" 

@(test)
test_parse_comment :: proc(t: ^testing.T) {
    p := Parser{str = "# this is a comment"}

    ret := next_token(&p)
    assert(ret == Ret.OK)
    assert(p.token == Token.UNSET)
}
