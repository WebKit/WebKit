
function testSyntax(script) {
    try {
        eval(script);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Bad error: " + String(error));
    }
}

function testSyntaxError(script, message) {
    var error = null;
    try {
        eval(script);
    } catch (e) {
        error = e;
    }
    if (!error)
        throw new Error("Expected syntax error not thrown");

    if (String(error) !== message)
        throw new Error("Bad error: " + String(error));
}

testSyntax("`Hello`");
testSyntax("(`Hello`)");
testSyntax("(`Hello`) + 42");
testSyntax("`\nHello\nWorld`");
testSyntax("`\nHello\n${World}`");
testSyntax("`${World}`");
testSyntax("`${World} Hello`");
testSyntax("`$ {World} Hello`");
testSyntax("`\\uFEFFtest`");
testSyntax("`\r\n`");
testSyntax("`\\r\\n`");
testSyntax("`\n`");
testSyntax("`\\n`");
testSyntax("`\u2028`");
testSyntax("`\\u2028`");
testSyntax("`\u2029`");
testSyntax("`\\u2029`");
testSyntax("`\\0`");
testSyntax("`\0`");
testSyntax("`\\x7f`");
testSyntax("(`Hello`) + 42");
testSyntax("`\\\n`");
testSyntax("`\\\r\n`");
testSyntax("`\\\r`");
testSyntax("Hello`bad escape sequence: \\unicode`");
testSyntax("Hello`\\00`");
testSyntax("Hello`\\01`");
testSyntax("Hello`\\1`");
testSyntax("Hello`\\xo`");
testSyntax("Hello`\\x0o`");
testSyntax("Hello`\\uo`");
testSyntax("Hello`\\u0o`");
testSyntax("Hello`\\u00o`");
testSyntax("Hello`\\u000o`");
testSyntax("Hello`\\u{o`");
testSyntax("Hello`\\u{0o`");
testSyntax("Hello`\\u{110000o`");

testSyntaxError("`Hello", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${expr}", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${}", "SyntaxError: Unexpected token '}'. Template literal expression cannot be empty.");
testSyntaxError("`Hello${expr} ${}", "SyntaxError: Unexpected token '}'. Template literal expression cannot be empty.");
testSyntaxError("`Hello${expr}", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${expr} ${expr}", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${expr`", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${expr} ${expr`", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello expr${`", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello${expr} expr${`", "SyntaxError: Unexpected EOF");
testSyntaxError("`Hello ${}`", "SyntaxError: Unexpected token '}'. Template literal expression cannot be empty.");
testSyntaxError("`Hello${expr} ${}`", "SyntaxError: Unexpected token '}'. Template literal expression cannot be empty.");
testSyntaxError("`\\07`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
for (var i = 1; i < 7; ++i) {
    testSyntaxError("`\\" + i + "`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
    testSyntaxError("`${expr}\\" + i + "`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
}
// \8 and \9 are not allowed.
for (var i = 8; i < 9; ++i) {
    testSyntaxError("`\\" + i + "`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
    testSyntaxError("`${expr}\\" + i + "`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
}
testSyntaxError("`\\x7`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`${expr}\\x7`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`\\x`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`${expr}\\x`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`\\u`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`${expr}\\u`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u2`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`${expr}\\u2`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u20`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`${expr}\\u20`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u202`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`${expr}\\u202`", "SyntaxError: \\u can only be followed by a Unicode character sequence");

testSyntaxError("`bad escape sequence: \\unicode`", "SyntaxError: \\u can only be followed by a Unicode character sequence");

testSyntaxError("`\\00`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
testSyntaxError("`\\01`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
testSyntaxError("`\\1`", "SyntaxError: The only valid numeric escape in strict mode is '\\0'");
testSyntaxError("`\\xo`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`\\x0o`", "SyntaxError: \\x can only be followed by a hex character sequence");
testSyntaxError("`\\uo`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u0o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u00o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u000o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u{o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u{0o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
testSyntaxError("`\\u{110000o`", "SyntaxError: \\u can only be followed by a Unicode character sequence");
