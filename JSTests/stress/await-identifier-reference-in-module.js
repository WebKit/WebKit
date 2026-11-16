function shouldThrowModuleSyntaxError(source) {
    let error;
    try {
        checkModuleSyntax(source);
    } catch (e) {
        error = e;
    }
    if (!String(error).startsWith("SyntaxError"))
        throw new Error(`\`${source}\` did not throw a SyntaxError: ${String(error)}`);
}

const wrappers = [
    expression => `function f() { ${expression}; }`,
    expression => `function f() { return ${expression}; }`,
    expression => `function f() { "use strict"; ${expression}; }`,
    expression => `function f() { function g() { ${expression}; } }`,
    expression => `function f() { return () => ${expression}; }`,
    expression => `function* f() { yield ${expression}; }`,
    expression => `(function () { ${expression}; });`,
    expression => `(() => ${expression});`,
    expression => `(() => { ${expression}; });`,
    expression => `({ m() { ${expression}; } });`,
    expression => `({ get x() { return ${expression}; } });`,
    expression => `class C { m() { ${expression}; } }`,
    expression => `class C { static m() { ${expression}; } }`,
    expression => `class C { constructor() { ${expression}; } }`,
    expression => `class C { x = ${expression}; }`,
    expression => `class C { static x = ${expression}; }`,
    expression => `class C { x = () => ${expression}; }`,
    expression => `class C { x = function () { ${expression}; }; }`,
    expression => `function f(x = ${expression}) { }`,
    expression => `(function (x = ${expression}) { });`,
    expression => `function f() { return (x = ${expression}) => { }; }`,
    expression => `({ m(x = ${expression}) { } });`,
    expression => `class C { m(x = ${expression}) { } }`,
    expression => `async function f() { function g() { ${expression}; } }`,
    expression => `async function f() { function g(x = ${expression}) { } }`,
    expression => `async function f() { class C { x = ${expression}; } }`,
    expression => `async function f() { class C { static x = ${expression}; } }`,
    expression => `(async () => class { x = ${expression}; });`,
];

const expressions = [
    "await",
    "(await)",
    "await.x",
    "await[0]",
    "await()",
    "await`x`",
    "await + 1",
    "1 + await",
    "await ** 2",
    "await ? 1 : 2",
    "await = 1",
    "await++",
    "typeof await",
    "[await]",
    "[await] = []",
    "({ x: await })",
    "({ x: await } = {})",
    "`${await}`",
    "f(await)",
    "new await",
];

for (let wrapper of wrappers) {
    for (let expression of expressions) {
        let source = wrapper(expression);
        checkScriptSyntax(source);
        shouldThrowModuleSyntaxError(source);
    }
}

const valid = [
    "await 1;",
    "async function f() { await 1; }",
    "function f() { return async () => await 1; }",
    "function f() { return { await: 1 }.await; }",
    "function f(x = { await: 1 }.await) { }",
    "class C { await() { return this.await; } static await = 1; }",
    "class C { x = async () => await 1; }",
    "async function f() { class C { [await 1] = 1; } }",
];

for (let source of valid)
    checkModuleSyntax(source);
