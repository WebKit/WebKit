// This test that the beginning of line (bol) optimization throws when we run out of stack space.

let expectedException = "SyntaxError: Invalid regular expression: regular expression too large";

function test()
{
    let source = Array(50000).join("(") + /(?:^|:|,)(?:\s*\[)+/g.toString() + Array(50000).join(")");
    RegExp(source);
}

try {
    test();
} catch(e) {
    if (e != expectedException)
       throw "Expected \"" + expectedException + "\" exception, but got \"" + e + "\"";
}
