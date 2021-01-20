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
function t() {
    for (42 of []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntaxError(`
async function t() {
    for await (42 of []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntaxError(`
function t() {
    for (42 in []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntaxError(`
function t() {
    for (new.target of []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntaxError(`
async function t() {
    for await (new.target of []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntaxError(`
function t() {
    for (new.target in []);
}
`, `SyntaxError: Left side of assignment is not a reference.`);

testSyntax(`
async function t() {
    for (hey.ok in []);
    for (hey.ok of []);
    for await (hey.ok of []);
}
`);

testSyntax(`
async function t() {
    for (hey[ok] in []);
    for (hey[ok] of []);
    for await (hey[ok] of []);
}
`);
