function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldNotThrow(source) {
    try {
        (0, eval)(source);
    } catch (error) {
        throw new Error(`\`${source}\` threw ${String(error)}`);
    }
}

function shouldThrowSyntaxError(source, message) {
    let error;
    try {
        (0, eval)(source);
    } catch (e) {
        error = e;
    }
    if (!(error instanceof SyntaxError))
        throw new Error(`\`${source}\` did not throw a SyntaxError: ${String(error)}`);
    if (error.message !== message)
        throw new Error(`\`${source}\` threw "${error.message}", expected "${message}"`);
}

// Function bodies using `await` as an identifier.
const bodies = [
    "var await;",
    "let await;",
    "const await = 1;",
    "var { await } = {};",
    "var { a: await } = {};",
    "var [await] = [];",
    "var [...await] = [];",
    "function await() {}",
    "class await {}",
    "await: ;",
    "await: for (;;) break await;",
    "for (var await in {}) {}",
    "for (let await of []) {}",
    "({ await });",
    "({ await } = {});",
    "[await] = [];",
    "await => {};",
    "(await) => {};",
    "(a = await) => {};",
    "function g(await) {}",
    "await;",
    "await = 1;",
    "\\u0061wait;",
    "var \\u0061wait;",
    "\\u0061wait: ;",
];

// Expressions containing a non-async function whose body is `body`.
const functions = [
    body => `function () { ${body} }`,
    body => `function* () { ${body} }`,
    body => `() => { ${body} }`,
    body => `{ method() { ${body} } }`,
    body => `{ *generator() { ${body} } }`,
    body => `{ get getter() { ${body} } }`,
    body => `{ set setter(value) { ${body} } }`,
    body => `class { constructor() { ${body} } }`,
    body => `class { method() { ${body} } }`,
    body => `class { static method() { ${body} } }`,
    body => `class { #method() { ${body} } }`,
    body => `class { field = function () { ${body} }; }`,
    body => `class { field = () => { ${body} }; }`,
    body => `class { static { (function () { ${body} })(); } }`,
    body => `function () { function nested() { ${body} } }`,
    body => `function () { () => { ${body} }; }`,
];

// Parameter lists in which `await` is reserved, with `value` as a default value.
const parameterLists = [
    value => `async function f(a = ${value}) {}`,
    value => `(async function (a = ${value}) {});`,
    value => `async function* f(a = ${value}) {}`,
    value => `(async function* (a = ${value}) {});`,
    value => `({ async method(a = ${value}) {} });`,
    value => `({ async *method(a = ${value}) {} });`,
    value => `(class { async method(a = ${value}) {} });`,
    value => `(class { static async *method(a = ${value}) {} });`,
    value => `async (a = ${value}) => {};`,
    value => `async function f({ a = ${value} }) {}`,
    value => `async function f([a = ${value}]) {}`,
    value => `async function f(a, b = ${value}, ...c) {}`,
    value => `async function f(a = (b = ${value}) => {}) {}`,
    value => `async function f() { (a = ${value}) => {}; }`,
    value => `async function f() { async (a = ${value}) => {}; }`,
    value => `"use strict"; async function f(a = ${value}) {}`,
];

for (const body of bodies) {
    for (const fn of functions)
        shouldNotThrow(parameterLists[0](fn(body)));
}

for (const parameterList of parameterLists) {
    for (const fn of functions) {
        shouldNotThrow(parameterList(fn("var await;")));
        shouldNotThrow(parameterList(fn("await: ;")));
    }
    // These functions are too short to be skipped by using the function cache when their body is parsed again.
    shouldNotThrow(parameterList("function(){var await}"));
    shouldNotThrow(parameterList("()=>{await:;}"));
}

shouldNotThrow("async function f(a = () => await) {}");
shouldNotThrow("async function f(a = () => ({ await })) {}");
shouldNotThrow("async function f(a = () => await => await) {}");
shouldNotThrow("async function f(a = () => (await) => await) {}");
shouldNotThrow("async function f(a = () => (b = await) => await) {}");
shouldNotThrow("async function f(a = () => \\u0061wait) {}");
shouldNotThrow("async function f(a = function (await) { await; }) {}");
shouldNotThrow("async function f(a = function await() { await; }) {}");
shouldNotThrow("async function f(a = function () { var await; }, b = function () { var await; }) {}");

// `await` is still an operator in the body of a nested async function.
shouldNotThrow("async function f(a = async function () { await 1; }) {}");
shouldNotThrow("async function f(a = async () => await 1) {}");
shouldNotThrow("async function f(a = { async method() { await 1; } }) {}");
shouldNotThrow("async function f(a = function () { async function nested() { await 1; } }) {}");
shouldNotThrow("async function f(a = function () { return async () => await 1; }) {}");

// `await` is still reserved in the rest of the parameter list, and in the body of the async function.
shouldThrowSyntaxError("async function f(a = function () { var await; }, await) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { var await; }, { await }) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { var await; }, b = await) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = function () { var await; }, b = await 1) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = function () { var await; }, b = await => {}) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = function () { var await; }, b = (await) => {}) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = () => { var await; }, b = await) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = () => await, b = await) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = class { method() { var await; } }, await) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = class { method() { var await; } [await]() {} }) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = function () { var await; }) { var await; }", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { await: ; }) { await: ; }", "Cannot use 'await' as a label in an async function.");
shouldThrowSyntaxError("async (a = function () { var await; }, await) => {};", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f() { (a = function () { var await; }, await) => {}; }", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f() { (a = function () { var await; }, b = await) => {}; }", "Cannot use 'await' within a parameter default expression.");

// `await` is still reserved in nested async functions and in class static blocks.
shouldThrowSyntaxError("async function f(a = async function () { var await; }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = async function* () { var await; }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = async () => { var await; }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = { async method() { var await; } }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = async function (await) {}) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = async function (b = await) {}) {}", "Cannot use 'await' within a parameter default expression.");
shouldThrowSyntaxError("async function f(a = function () { async function nested() { var await; } }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { async function nested(await) {} }) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { async function nested() { (await) => {}; } }) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { async (await) => {}; }) {}", "Cannot use 'await' as a parameter name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { return async () => { var await; }; }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = class { static { var await; } }) {}", "Cannot use 'await' as a variable name in an async function.");
shouldThrowSyntaxError("async function f(a = function () { class C { static { var await; } } }) {}", "Cannot use 'await' as a variable name in a static block.");

{
    let result;
    async function f(a = function () { var await = 42; return await; }) { result = a(); }
    f();
    shouldBe(result, 42);
}

{
    let result;
    async function* f(a = () => { let count = 0; await: for (;;) { if (++count === 3) break await; } return count; }) { result = a(); }
    f().next();
    shouldBe(result, 3);
}

{
    let result;
    let object = { async method(a = class { method(await) { return await + 1; } }) { result = new a().method(41); } };
    object.method();
    shouldBe(result, 42);
}

{
    let result;
    let f = async (a = function(){var await = 42; return await}) => { result = a(); };
    f();
    shouldBe(result, 42);
}

{
    let result;
    async function f() {
        let g = (a = function () { function await() { return 42; } return await(); }) => a();
        result = g();
    }
    f();
    shouldBe(result, 42);
}

{
    let result;
    let await = 42;
    async function f(a = () => await) { result = a(); }
    f();
    shouldBe(result, 42);
}

{
    let AsyncFunction = (async function () { }).constructor;
    let result;
    let f = new AsyncFunction("callback", "a = function () { var await = 42; return await; }", "callback(a());");
    f(value => { result = value; });
    shouldBe(result, 42);
}
