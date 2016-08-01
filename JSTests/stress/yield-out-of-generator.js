// <https://webkit.org/b/158460> Clarify SyntaxErrors around yield and unskip tests
//@ skip

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
yield;
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
yield*;
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
yield 0;
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
yield* 0;
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function hello() {
    yield;
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function hello() {
    yield*;
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function hello() {
    yield 0;
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function hello() {
    yield* 0;
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function *gen() {
    function hello() {
        yield;
    }
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function *gen() {
    function hello() {
        yield*;
    }
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function *gen() {
    function hello() {
        yield 0;
    }
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

testSyntaxError(`
function *gen() {
    function hello() {
        yield* 0;
    }
}
`, "SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.");

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
