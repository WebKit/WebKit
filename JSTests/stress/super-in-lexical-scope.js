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
        throw new Error("Bad error: " + String(error) + "(Expected: " + message + ")");
}

testSyntaxError(`super()`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`super.hello()`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
{
    super();
}
`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
{
    super.hello();
}
`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
function test()
{
    super();
}
`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
function test()
{
    super.hello();
}
`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
function test()
{
    {
        super();
    }
}
`, `SyntaxError: super is not valid in this context.`);
testSyntaxError(`
function test()
{
    {
        super.hello();
    }
}
`, `SyntaxError: super is not valid in this context.`);
