// This test that the beginning of line (bol) optimization throws when we run out of stack space.
//@ requireOptions("-e", "let arrayLength=25000") if $memoryLimited

arrayLength = typeof(arrayLength) === 'undefined' ? 50000 : arrayLength;

let expectedException = "SyntaxError: Invalid regular expression: regular expression too large";

function test()
{
    let source = Array(arrayLength).join("(") + /(?:^|:|,)(?:\s*\[)+/g.toString() + Array(arrayLength).join(")");
    RegExp(source);
}

try {
    test();
} catch(e) {
    if (e != expectedException)
       throw "Expected \"" + expectedException + "\" exception, but got \"" + e + "\"";
}
