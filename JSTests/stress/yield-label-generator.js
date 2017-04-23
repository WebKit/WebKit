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
function *test() {
    {
        yield: for (var i = 0; i < 1000; ++i) {
            break yield;
        }
    }
}
`, `SyntaxError: Cannot use 'yield' as a label in a generator function.`);

testSyntaxError(`
function *test() {
    {
        label: for (var i = 0; i < 1000; ++i) {
            break yield;
        }
    }
}
`, `SyntaxError: Unexpected keyword 'yield'. Expected an identifier as the target for a break statement.`);

testSyntaxError(`
function *test() {
    {
        label: for (var i = 0; i < 1000; ++i) {
            continue yield;
        }
    }
}
`, `SyntaxError: Unexpected keyword 'yield'. Expected an identifier as the target for a continue statement.`)

testSyntax(`
function *test() {
    "OK" ? yield : "NG";  // This is not a label.
}
`);
