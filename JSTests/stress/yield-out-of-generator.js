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

testSyntax(`
yield;
`);

testSyntaxError(`
yield*;
`, "SyntaxError: Unexpected token ';'");

testSyntaxError(`
yield 0;
`, "SyntaxError: Unexpected number '0'");

testSyntax(`
yield* 0;
`);

testSyntax(`
function hello() {
    yield;
}
`);

testSyntaxError(`
function hello() {
    yield*;
}
`, "SyntaxError: Unexpected token ';'");

testSyntaxError(`
function hello() {
    yield 0;
}
`, "SyntaxError: Unexpected number '0'");

testSyntax(`
function hello() {
    yield* 0;
}
`);

testSyntax(`
function *gen() {
    function hello() {
        yield;
    }
}
`);

testSyntaxError(`
function *gen() {
    function hello() {
        yield*;
    }
}
`, "SyntaxError: Unexpected token ';'");

testSyntaxError(`
function *gen() {
    function hello() {
        yield 0;
    }
}
`, "SyntaxError: Unexpected number '0'");

testSyntax(`
function *gen() {
    function hello() {
        yield* 0;
    }
}
`);

testSyntax(`
function *gen() {
    yield;
}
`);

testSyntaxError(`
function *gen() {
    yield*;
}
`, "SyntaxError: Unexpected token '*'");

testSyntax(`
function *gen() {
    yield 0;
}
`);

testSyntax(`
function *gen() {
    yield* 0;
}
`);

testSyntax(`
function *gen() {
    {
        let i = 30;
        function ok() {
            return i;
        }
        yield;
    }
}
`);

testSyntaxError(`
function *gen() {
    {
        let i = 30;
        function ok() {
            return i;
        }
        yield*;
    }
}
`, "SyntaxError: Unexpected token '*'");

testSyntax(`
function *gen() {
    {
        let i = 30;
        function ok() {
            return i;
        }
        yield 0;
    }
}
`);

testSyntax(`
function *gen() {
    {
        let i = 30;
        function ok() {
            return i;
        }
        yield* 0;
    }
}
`);
