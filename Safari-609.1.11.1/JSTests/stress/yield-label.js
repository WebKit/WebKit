// http://ecma-international.org/ecma-262/6.0/#sec-identifiers-static-semantics-early-errors
// If the "yield" label is used under the sloppy mode and the context is not
// a generator context, we can use "yield" as a label.

(function () {
    {
        yield: for (var i = 0; i < 1000; ++i) {
            break yield;
        }
    }
    {
        yield: for (var i = 0; i < 1000; ++i) {
            continue yield;
        }
    }
}());


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
function test() {
    "use strict";
    {
        yield: for (var i = 0; i < 1000; ++i) {
            break yield;
        }
    }
}
`, `SyntaxError: Cannot use 'yield' as a label in strict mode.`);

testSyntaxError(`
function test() {
    "use strict";
    {
        label: for (var i = 0; i < 1000; ++i) {
            break yield;
        }
    }
}
`, `SyntaxError: Unexpected keyword 'yield'. Expected an identifier as the target for a break statement.`);

testSyntaxError(`
function test() {
    "use strict";
    {
        label: for (var i = 0; i < 1000; ++i) {
            continue yield;
        }
    }
}
`, `SyntaxError: Unexpected keyword 'yield'. Expected an identifier as the target for a continue statement.`)
