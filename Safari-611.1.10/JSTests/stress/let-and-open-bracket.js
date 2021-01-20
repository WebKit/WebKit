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

testSyntaxError(`
if (false) let [ok] = 40;
`, `SyntaxError: Unexpected token '['. Cannot use lexical declaration in single-statement context.`);

testSyntaxError(`
if (false) let
[ok] = 40;
`, `SyntaxError: Unexpected token '['. Cannot use lexical declaration in single-statement context.`);

testSyntax(`
if (false) {
    let [ok] = 40;
}
`);
