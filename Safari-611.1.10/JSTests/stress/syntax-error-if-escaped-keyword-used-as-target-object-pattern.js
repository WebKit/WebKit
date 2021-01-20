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
        throw new Error(`Bad error: ${String(error)}`);
}

testSyntaxError(String.raw`
var { bre\u0061k } = { break: 42 };
`, `SyntaxError: Cannot use abbreviated destructuring syntax for keyword 'break'.`);

testSyntaxError(String.raw`
var { break: bre\u0061k } = { break: 42 };
`, String.raw`SyntaxError: Unexpected escaped characters in keyword token: 'bre\u0061k'`);
