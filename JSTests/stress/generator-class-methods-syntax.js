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
class Cocoa {
    *constructor()
    {
    }
}
`, `SyntaxError: Cannot declare a generator function named 'constructor'.`);

testSyntax(`
class Cocoa {
    *ok()
    {
        yield 42;
    }
}
`);

testSyntax(`
class Cocoa {
    static *ok()
    {
        yield 42;
    }
}
`);
