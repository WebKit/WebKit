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
        throw new Error(`Bad error: ${String(error)}`);
}

async function testSyntax(script, message) {
    var error = null;
    try {
        await eval(script);
    } catch (e) {
        error = e;
    }
    if (error) {
        if (error instanceof SyntaxError)
            throw new Error("Syntax error thrown");
    }
}

testSyntaxError(`import)`, `SyntaxError: Unexpected token ')'. import call expects exactly one argument.`);
testSyntaxError(`new import(`, `SyntaxError: Cannot use new with import.`);
testSyntaxError(`import.hello()`, `SyntaxError: Unexpected token '.'. import call expects exactly one argument.`);
testSyntaxError(`import[`, `SyntaxError: Unexpected token '['. import call expects exactly one argument.`);
testSyntaxError(`import<`, `SyntaxError: Unexpected token '<'. import call expects exactly one argument.`);

testSyntaxError(`import()`, `SyntaxError: Unexpected token ')'`);
testSyntaxError(`import(a, b)`, `SyntaxError: Unexpected token ','. import call expects exactly one argument.`);
testSyntaxError(`import(a, b, c)`, `SyntaxError: Unexpected token ','. import call expects exactly one argument.`);
testSyntaxError(`import(...a)`, `SyntaxError: Unexpected token '...'`);
testSyntaxError(`import(,a)`, `SyntaxError: Unexpected token ','`);
testSyntaxError(`import(,)`, `SyntaxError: Unexpected token ','`);
testSyntaxError(`import("Hello";`, `SyntaxError: Unexpected token ';'. import call expects exactly one argument.`);
testSyntaxError(`import("Hello"];`, `SyntaxError: Unexpected token ']'. import call expects exactly one argument.`);
testSyntaxError(`import("Hello",;`, `SyntaxError: Unexpected token ','. import call expects exactly one argument.`);
testSyntaxError(`import("Hello", "Hello2";`, `SyntaxError: Unexpected token ','. import call expects exactly one argument.`);


testSyntaxError(`import = 42`, `SyntaxError: Unexpected token '='. import call expects exactly one argument.`);
testSyntaxError(`[import] = 42`, `SyntaxError: Unexpected token ']'. import call expects exactly one argument.`);
testSyntaxError(`{import} = 42`, `SyntaxError: Unexpected token '}'. import call expects exactly one argument.`);
testSyntaxError(`let import = 42`, `SyntaxError: Unexpected keyword 'import'`);
testSyntaxError(`var import = 42`, `SyntaxError: Cannot use the keyword 'import' as a variable name.`);
testSyntaxError(`const import = 42`, `SyntaxError: Cannot use the keyword 'import' as a lexical variable name.`);

(async function () {
    await testSyntax(`import("./import-tests/cocoa.js")`);
    await testSyntax(`import("./import-tests/../import-tests/cocoa.js")`);
    await testSyntax(`import("./import-tests/../import-tests/cocoa.js").then(() => { })`);
    await testSyntax(`(import("./import-tests/../import-tests/cocoa.js").then(() => { }))`);
    await testSyntax(`(import("./import-tests/../import-tests/cocoa.js"))`);
    await testSyntax(`import("./import-tests/../import-tests/cocoa.js").catch(() => { })`);
    await testSyntax(`(import("./import-tests/../import-tests/cocoa.js").catch(() => { }))`);
}()).catch((error) => {
    print(String(error));
    abort();
});
