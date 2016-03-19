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

testSyntaxError(`super()`, `SyntaxError: 'super' is only valid inside a function or an 'eval' inside a function.`);
testSyntaxError(`super.hello()`, `SyntaxError: 'super' is only valid inside a function or an 'eval' inside a function.`);
testSyntaxError(`
{
    super();
}
`, `SyntaxError: 'super' is only valid inside a function or an 'eval' inside a function.`);
testSyntaxError(`
{
    super.hello();
}
`, `SyntaxError: 'super' is only valid inside a function or an 'eval' inside a function.`);
testSyntaxError(`
function test()
{
    super();
}
`, `SyntaxError: Cannot call super() outside of a class constructor.`);
testSyntaxError(`
function test()
{
    super.hello();
}
`, `SyntaxError: super can only be used in a method of a derived class.`);
testSyntaxError(`
function test()
{
    {
        super();
    }
}
`, `SyntaxError: Cannot call super() outside of a class constructor.`);
testSyntaxError(`
function test()
{
    {
        super.hello();
    }
}
`, `SyntaxError: super can only be used in a method of a derived class.`);
