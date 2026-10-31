// Microbenchmark for RegExp.test() with unicode flag inlining.
// Only literal patterns are inlineable (character classes, dot, \d, and
// ignoreCase all require callFrame which prevents inlining).

function testSingleChar(s) {
    return /x/u.test(s);
}
noInline(testSingleChar);

function testLiteral3(s) {
    return /abc/u.test(s);
}
noInline(testLiteral3);

function testLiteral5(s) {
    return /hello/u.test(s);
}
noInline(testLiteral5);

function testLiteral8(s) {
    return /function/u.test(s);
}
noInline(testLiteral8);

var count = 1e5;

for (var i = 0; i < count; ++i) {
    testSingleChar("abcxdef");
    testSingleChar("abcdef");
}

for (var i = 0; i < count; ++i) {
    testLiteral3("xabcx");
    testLiteral3("xyz");
}

for (var i = 0; i < count; ++i) {
    testLiteral5("say hello world");
    testLiteral5("xyz");
}

for (var i = 0; i < count; ++i) {
    testLiteral8("var function foo");
    testLiteral8("xyz");
}
