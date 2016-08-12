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
class Hello {
    get *gen() {
    }
}
`, `SyntaxError: Unexpected token '*'. Expected an opening '(' before a method's parameter list.`);


testSyntaxError(`
class Hello {
    set *gen(value) {
    }
}
`, `SyntaxError: Unexpected token '*'. Expected an opening '(' before a method's parameter list.`);

testSyntaxError(`
function ** gen() { }
`, `SyntaxError: Unexpected token '*'`);

// http://ecma-international.org/ecma-262/6.0/#sec-arrow-function-definitions-static-semantics-early-errors
testSyntaxError(`
var value = () => {
    yield
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.`);

testSyntaxError(`
var value = (val = yield) => {
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.`);

testSyntaxError(`
function *gen() {
    function ng(val = yield) {
    }
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.`);

testSyntaxError(`
function *gen() {
    var ng = (val = yield) => {
    }
}
`, `SyntaxError: Unexpected token '=>'. Expected ';' after variable declaration.`);

// http://ecma-international.org/ecma-262/6.0/#sec-generator-function-definitions-static-semantics-early-errors
testSyntaxError(`
function gen(val = yield) {
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression out of generator.`);

testSyntaxError(`
function *gen(val = yield) {
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression within parameters.`);

testSyntaxError(`
function *gen(val = yield 20) {
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression within parameters.`);

testSyntaxError(`
function *gen(val = yield * g) {
}
`, `SyntaxError: Unexpected keyword 'yield'. Cannot use yield expression within parameters.`);


testSyntax(`
function *gen(g = function *() { yield  }) {
}
`);
