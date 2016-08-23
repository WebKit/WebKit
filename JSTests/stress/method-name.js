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
class Hello {
    hello hello() { }
}
`, `SyntaxError: Unexpected identifier 'hello'. Expected an opening '(' before a method's parameter list.`);

testSyntaxError(`
let obj = {
    hello hello() { }
}
`, `SyntaxError: Unexpected identifier 'hello'. Expected a ':' following the property name 'hello'.`);

testSyntaxError(`
class Hello {
    get hello hello() { }
}
`, `SyntaxError: Unexpected identifier 'hello'. Expected a parameter list for getter definition.`);

testSyntaxError(`
class Hello {
    set hello hello(v) { }
}
`, `SyntaxError: Unexpected identifier 'hello'. Expected a parameter list for setter definition.`);
