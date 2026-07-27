// This test that the beginning of line (bol) optimization throws when we run out of stack space.
//@ exclusive!
//@ requireOptions("-e", "let arrayLength=25000") if $memoryLimited

arrayLength = typeof(arrayLength) === 'undefined' ? 50000 : arrayLength;

let expectedException = "SyntaxError: Invalid regular expression: regular expression too large";

function test()
{
    // Use non-capturing groups so that the nesting depth, not the capture count, is what the
    // pattern stresses. With capturing groups the parser hits its capture limit first, and which
    // of the two errors comes out depends on arrayLength (25000 when $memoryLimited vs 50000),
    // which is not what this test is about.
    let source = Array(arrayLength).join("(?:") + /(?:^|:|,)(?:\s*\[)+/g.toString() + Array(arrayLength).join(")");
    RegExp(source);
}

try {
    test();
} catch(e) {
    if (e != expectedException)
       throw "Expected \"" + expectedException + "\" exception, but got \"" + e + "\"";
}
