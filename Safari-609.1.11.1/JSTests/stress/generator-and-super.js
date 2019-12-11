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
class Base {
    hello()
    {
        print("Hello");
    }

    *ok()
    {
        super.hello();
    }
}

class Hello extends Base {
    *gen()
    {
        super();
    }
}
`, `SyntaxError: super is not valid in this context.`);


testSyntaxError(`
function *hello()
{
    super.hello();
}
`, `SyntaxError: super is not valid in this context.`);
