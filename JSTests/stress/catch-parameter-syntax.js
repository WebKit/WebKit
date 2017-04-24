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
(function () {
    try {
    } catch ([a, a]) {
    }
})
`, `SyntaxError: Unexpected identifier 'a'. Cannot declare a lexical variable twice: 'a'.`);

testSyntaxError(`
(function () {
    try {
    } catch ({ a, b:a }) {
    }
})
`, `SyntaxError: Unexpected identifier 'a'. Cannot declare a lexical variable twice: 'a'.`);

testSyntax(`
(function () {
    try {
    } catch (let) {
    }
})
`, ``);

testSyntax(`
(function () {
    try {
    } catch ([let]) {
    }
})
`, ``);

testSyntaxError(`
(function () {
    'use strict';
    try {
    } catch (let) {
    }
})
`, `SyntaxError: Cannot use the keyword 'let' as a catch parameter name.`);

testSyntaxError(`
(function () {
    'use strict';
    try {
    } catch ([let]) {
    }
})
`, `SyntaxError: Cannot use the keyword 'let' as a catch parameter name.`);


testSyntax(`
(function () {
    try {
    } catch (yield) {
    }
})
`);

testSyntax(`
(function () {
    try {
    } catch ([yield]) {
    }
})
`);

testSyntaxError(`
(function () {
    'use strict';
    try {
    } catch (yield) {
    }
})
`, `SyntaxError: Cannot use the keyword 'yield' as a catch parameter name.`);

testSyntaxError(`
(function () {
    'use strict';
    try {
    } catch ([yield]) {
    }
})
`, `SyntaxError: Cannot use the keyword 'yield' as a catch parameter name.`);

testSyntaxError(`
(function () {
    try {
    } catch (yield = 20) {
    }
})
`, `SyntaxError: Unexpected token '='. Expected ')' to end a 'catch' target.`);

testSyntax(`
(function () {
    try {
    } catch ([...yield]) {
    }
})
`);

testSyntax(`
(function () {
    try {
    } catch ([yield = 30]) {
    }
})
`);

testSyntax(`
(function () {
    try {
    } catch ({ yield = 30 }) {
    }
})
`);

testSyntaxError(`
(function () {
    try {
    } catch (...Hello) {
    }
})
`, `SyntaxError: Unexpected token '...'. Expected a parameter pattern or a ')' in parameter list.`);

testSyntaxError(`
(function *() {
    try {
    } catch (yield) {
    }
})
`, `SyntaxError: Cannot use the keyword 'yield' as a catch parameter name.`);

testSyntax(`
(function *() {
    try {
    } catch ({ value = yield 42 }) {
    }
})
`);

testSyntax(`
(function *() {
    try {
    } catch ({ value = yield }) {
    }
})
`);
