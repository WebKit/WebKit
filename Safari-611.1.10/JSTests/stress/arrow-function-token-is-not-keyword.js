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

testSyntaxError(`V={=>`, `SyntaxError: Unexpected token '=>'. Expected a property name.`);
