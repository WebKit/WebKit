// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function testSyntax(script) {
    try {
        eval(script);
    } catch (error) {
        if (error instanceof SyntaxError)
            throw new Error("Bad error: " + String(error) + "\n       evaluating `" + script + "`");
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
        throw new Error("Expected syntax error not thrown\n       evaluating `" + script + "`");

    if (typeof message === "string" && String(error) !== message)
        throw new Error("Bad error: " + String(error) + "\n       evaluating `" + script + "`");
}

(function testTopLevelAsyncAwaitSyntaxSloppyMode() {
    testSyntax(`({async: 1})`);
    testSyntax(`var asyncFn = async function() { await 1; };`);
    testSyntax(`var asyncFn = async function withName() { await 1; };`);
    testSyntax(`var asyncFn = async () => await 'test';`);
    testSyntax(`var asyncFn = async x => await x + 'test';`);
    testSyntax(`function foo(fn) { fn({ async: true }); }`);
    testSyntax(`async function asyncFn() { await 1; }`);
    testSyntax(`var O = { async method() { await 1; } };`);
    testSyntax(`var O = { async ['meth' + 'od']() { await 1; } };`);
    testSyntax(`var O = { async 'method'() { await 1; } };`);
    testSyntax(`var O = { async 0() { await 1; } };`);
    testSyntax(`var O = { async function() {} };`);
    testSyntax(`class C { async method() { await 1; } };`);
    testSyntax(`class C { async ['meth' + 'od']() { await 1; } };`);
    testSyntax(`class C { async 'method'() { await 1; } };`);
    testSyntax(`class C { async 0() { await 1; } };`);
    testSyntax(`var asyncFn = async({ foo = 1 }) => foo;`);
    testSyntax(`var asyncFn = async({ foo = 1 } = {}) => foo;`);
    testSyntax(`function* g() { var f = async(yield); }`);
    testSyntax(`function* g() { var f = async(x = yield); }`);
    testSyntax(`class C { async ['function']() {} }`);
    testSyntax(`class C {}; class C2 extends C { async ['function']() {} }`);
    testSyntax(`class C { static async ['function']() {} }`);
    testSyntax(`class C {}; class C2 extends C { static async ['function']() {} }`);
    testSyntax(`class C { async function() {} }`);
    testSyntax(`class C {}; class C2 extends C { async function() {} }`);
    testSyntax(`class C { static async function() {} }`);
    testSyntax(`class C {}; class C2 extends C { static async function() {} }`);
    testSyntax(`class C { async 'function'() {} }`);
    testSyntax(`class C {}; class C2 extends C { async 'function'() {} }`);
    testSyntax(`class C { static async 'function'() {} }`);
    testSyntax(`class C {}; class C2 extends C { static async 'function'() {} }`);
})();

(function testTopLevelAsyncAwaitSyntaxStrictMode() {
    testSyntax(`"use strict"; ({async: 1})`);
    testSyntax(`"use strict"; var asyncFn = async function() { await 1; };`);
    testSyntax(`"use strict"; var asyncFn = async function withName() { await 1; };`);
    testSyntax(`"use strict"; var asyncFn = async () => await 'test';`);
    testSyntax(`"use strict"; var asyncFn = async x => await x + 'test';`);
    testSyntax(`"use strict"; function foo(fn) { fn({ async: true }); }`);
    testSyntax(`"use strict"; async function asyncFn() { await 1; }`);
    testSyntax(`"use strict"; var O = { async method() { await 1; } };`);
    testSyntax(`"use strict"; var O = { async ['meth' + 'od']() { await 1; } };`);
    testSyntax(`"use strict"; var O = { async 'method'() { await 1; } };`);
    testSyntax(`"use strict"; var O = { async 0() { await 1; } };`);
    testSyntax(`"use strict"; class C { async method() { await 1; } };`);
    testSyntax(`"use strict"; class C { async ['meth' + 'od']() { await 1; } };`);
    testSyntax(`"use strict"; class C { async 'method'() { await 1; } };`);
    testSyntax(`"use strict"; class C { async 0() { await 1; } };`);
    testSyntax(`"use strict"; var asyncFn = async({ foo = 1 }) => foo;`);
    testSyntax(`"use strict"; var asyncFn = async({ foo = 1 } = {}) => foo;`);
    testSyntax(`"use strict"; function* g() { var f = async(yield); }`);
    testSyntax(`"use strict"; function* g() { var f = async(x = yield); }`);
    testSyntax(`"use strict"; class C { async ['function']() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { async ['function']() {} }`);
    testSyntax(`"use strict"; class C { static async ['function']() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { static async ['function']() {} }`);
    testSyntax(`"use strict"; class C { async function() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { async function() {} }`);
    testSyntax(`"use strict"; class C { async function() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { async function() {} }`);
    testSyntax(`"use strict"; class C { static async function() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { static async function() {} }`);
    testSyntax(`"use strict"; class C { async 'function'() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { async 'function'() {} }`);
    testSyntax(`"use strict"; class C { static async 'function'() {} }`);
    testSyntax(`"use strict"; class C {}; class C2 extends C { static async 'function'() {} }`);
})();

(function testNestedAsyncAwaitSyntax() {
    var contextData = [
        { prefix: "function outerFunction() { ", suffix: " }" },
        { prefix: "function* outerGenerator() { ", suffix: " }" },
        { prefix: "var outerFuncExpr = function() { ", suffix: " };" },
        { prefix: "var outerGenExpr = function*() { ", suffix: " };" },
        { prefix: "var outerObject = { outerMethod() { ", suffix: " } };" },
        { prefix: "var outerObject = { *outerGenMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { outerMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { *outerGenMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { static outerStaticMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { static *outerStaticGenMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { outerMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { *outerGenMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { static outerStaticMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { static *outerStaticGenMethod() { ", suffix: " } };" },
        { prefix: "var outerArrow = () => { ", suffix: " };" },
        { prefix: "async function outerAsyncFunction() { ", suffix: " }" },
        { prefix: "var outerAsyncFuncExpr = async function() { ", suffix: " };" },
        { prefix: "var outerAsyncArrowFunc = async () => { ", suffix: " };" },
        { prefix: "var outerObject = { async outerAsyncMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { async outerAsyncMethod() { ", suffix: " } };" },
        { prefix: "var outerClassExpr = class C { static async outerStaticAsyncMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { async outerAsyncMethod() { ", suffix: " } };" },
        { prefix: "class outerClass { static async outerStaticAsyncMethod() { ", suffix: " } };" },
    ];

    var testData = [
        `var async = 1; return async;`,
        `let async = 1; return async;`,
        `const async = 1; return async;`,
        `function async() {} return async();`,
        `var async = async => async; return async();`,
        `function foo() { var await = 1; return await; }`,
        `function foo(await) { return await; }`,
        `function* foo() { var await = 1; return await; }`,
        `function* foo(await) { return await; }`,
        `var f = () => { var await = 1; return await; }`,
        `var O = { method() { var await = 1; return await; } };`,
        `var O = { method(await) { return await; } };`,
        `var O = { *method() { var await = 1; return await; } };`,
        `var O = { *method(await) { return await; } };`,

        `(function await() {})`,
    ];

    for (let context of contextData) {
        for (let test of testData) {
            let script = context.prefix + test + context.suffix;
            testSyntax(script);
            testSyntax(`"use strict"; ${script}`);
        }
    }
})();


(function testTopLevelAsyncAwaitSyntaxSloppyMode() {
    testSyntaxError(`({ async 
        foo() {} })`);
    testSyntaxError(`({async =  1})`);
    testSyntaxError(`var asyncFn = async function await() {}`);
    testSyntaxError(`var asyncFn = async () => var await = 'test';`);
    testSyntaxError(`var asyncFn = async () => { var await = 'test'; };`);
    testSyntaxError(`var asyncFn = async await => await + 'test'`);
    testSyntaxError(`var asyncFn = async function(await) {}`);
    testSyntaxError(`var asyncFn = async function withName(await) {}`);
    testSyntaxError(`var asyncFn = async (await) => 'test';`);
    testSyntaxError(`async function asyncFunctionDeclaration(await) {}`);

    testSyntaxError(`var outerObject = { async method(a, a) {} }`);
    testSyntaxError(`var outerObject = { async ['meth' + 'od'](a, a) {} }`);
    testSyntaxError(`var outerObject = { async 'method'(a, a) {} }`);
    testSyntaxError(`var outerObject = { async 0(a, a) {} }`);

    testSyntaxError(`var outerObject = { async method(a, {a}) {} }`);
    testSyntaxError(`var outerObject = { async method({a}, a) {} }`);
    testSyntaxError(`var outerObject = { async method({a}, {a}) {} }`);
    testSyntaxError(`var outerObject = { async method(a, ...a) {} }`);
    testSyntaxError(`var outerObject = { async method({a}, ...a) {} }`);
    testSyntaxError(`var outerObject = { async method(a, ...a) {} }`);
    testSyntaxError(`var outerObject = { async method({a, ...a}) {} }`);
    testSyntaxError(`var outerObject = { func: async function(a, {a}) {} }`);
    testSyntaxError(`var outerObject = { func: async function({a}, a) {} }`);
    testSyntaxError(`var outerObject = { func: async function({a}, {a}) {} }`);
    testSyntaxError(`var outerObject = { func: async function(a, ...a) {} }`);
    testSyntaxError(`var outerObject = { func: async function({a}, ...a) {} }`);
    testSyntaxError(`var outerObject = { func: async function(a, ...a) {} }`);
    testSyntaxError(`var outerObject = { func: async function({a, ...a}) {} }`);

    testSyntaxError(`var asyncArrowFn = async() => await;`);

    testSyntaxError(`var O = { *async asyncGeneratorMethod() {} };`);
    testSyntaxError(`var O = { async asyncGeneratorMethod*() {} };`);

    testSyntaxError(`var asyncFn = async function(x = await 1) { return x; }`);
    testSyntaxError(`async function f(x = await 1) { return x; }`);
    testSyntaxError(`var f = async(x = await 1) => x;`);
    testSyntaxError(`var O = { async method(x = await 1) { return x; } };`);

    testSyntaxError(`function* outerGenerator() { var asyncArrowFn = async yield => 1; }`);
    testSyntaxError(`function* outerGenerator() { var asyncArrowFn = async(yield) => 1; }`);
    testSyntaxError(`function* outerGenerator() { var asyncArrowFn = async(x = yield) => 1; }`);
    testSyntaxError(`function* outerGenerator() { var asyncArrowFn = async({x = yield}) => 1; }`);

    testSyntaxError(`class C { async constructor() {} }`);
    testSyntaxError(`class C {}; class C2 extends C { async constructor() {} }`);
    testSyntaxError(`class C { static async prototype() {} }`);
    testSyntaxError(`class C {}; class C2 extends C { static async prototype() {} }`);

    testSyntaxError(`var f = async() => ((async(x = await 1) => x)();`);

    // Henrique Ferreiro's bug (tm)
    testSyntaxError(`(async function foo1() { } foo2 => 1)`);
    testSyntaxError(`(async function foo3() { } () => 1)`);
    testSyntaxError(`(async function foo4() { } => 1)`);
    testSyntaxError(`(async function() { } foo5 => 1)`);
    testSyntaxError(`(async function() { } () => 1)`);
    testSyntaxError(`(async function() { } => 1)`);
    testSyntaxError(`(async.foo6 => 1)`);
    testSyntaxError(`(async.foo7 foo8 => 1)`);
    testSyntaxError(`(async.foo9 () => 1)`);
    testSyntaxError(`(async().foo10 => 1)`);
    testSyntaxError(`(async().foo11 foo12 => 1)`);
    testSyntaxError(`(async().foo13 () => 1)`);
    testSyntaxError(`(async['foo14'] => 1)`);
    testSyntaxError(`(async['foo15'] foo16 => 1)`);
    testSyntaxError(`(async['foo17'] () => 1)`);
    testSyntaxError(`(async()['foo18'] => 1)`);
    testSyntaxError(`(async()['foo19'] foo20 => 1)`);
    testSyntaxError(`(async()['foo21'] () => 1`);
    testSyntaxError("(async`foo22` => 1)");
    testSyntaxError("(async`foo23` foo24 => 1)");
    testSyntaxError("(async`foo25` () => 1)");
    testSyntaxError("(async`foo26`.bar27 => 1)");
    testSyntaxError("(async`foo28`.bar29 foo30 => 1)");
    testSyntaxError("(async`foo31`.bar32 () => 1)");

    // assert that errors are still thrown for calls that may have been async functions
    testSyntaxError(`function async() {}
                     async({ foo33 = 1 })`);
})();

(function testTopLevelAsyncAwaitSyntaxStrictMode() {
    testSyntaxError(`"use strict"; ({ async 
        foo() {} })`);
    testSyntaxError(`"use strict"; ({async =  1})`);
    testSyntaxError(`"use strict"; var asyncFn = async function await() {}`);
    testSyntaxError(`"use strict"; var asyncFn = async () => var await = 'test';`);
    testSyntaxError(`"use strict"; var asyncFn = async () => { var await = 'test'; };`);
    testSyntaxError(`"use strict"; var asyncFn = async await => await + 'test'`);
    testSyntaxError(`"use strict"; var asyncFn = async function(await) {}`);
    testSyntaxError(`"use strict"; var asyncFn = async function withName(await) {}`);
    testSyntaxError(`"use strict"; var asyncFn = async (await) => 'test';`);
    testSyntaxError(`"use strict"; async function asyncFunctionDeclaration(await) {}`);

    testSyntaxError(`"use strict"; var outerObject = { async method(a, a) {} }`);
    testSyntaxError(`"use strict"; var outerObject = { async ['meth' + 'od'](a, a) {} }`);
    testSyntaxError(`"use strict"; var outerObject = { async 'method'(a, a) {} }`);
    testSyntaxError(`"use strict"; var outerObject = { async 0(a, a) {} }`);

    testSyntaxError(`"use strict"; var asyncArrowFn = async() => await;`);

    testSyntaxError(`"use strict"; var O = { *async asyncGeneratorMethod() {} };`);
    testSyntaxError(`"use strict"; var O = { async asyncGeneratorMethod*() {} };`);

    testSyntax(`"use strict"; var O = { async function() {} };`);

    testSyntaxError(`"use strict"; var asyncFn = async function(x = await 1) { return x; }`);
    testSyntaxError(`"use strict"; async function f(x = await 1) { return x; }`);
    testSyntaxError(`"use strict"; var f = async(x = await 1) => x;`);
    testSyntaxError(`"use strict"; var O = { async method(x = await 1) { return x; } };`);

    testSyntaxError(`"use strict"; function* outerGenerator() { var asyncArrowFn = async yield => 1; }`);
    testSyntaxError(`"use strict"; function* outerGenerator() { var asyncArrowFn = async(yield) => 1; }`);
    testSyntaxError(`"use strict"; function* outerGenerator() { var asyncArrowFn = async(x = yield) => 1; }`);
    testSyntaxError(`"use strict"; function* outerGenerator() { var asyncArrowFn = async({x = yield}) => 1; }`);

    testSyntaxError(`"use strict"; class C { async constructor() {} }`);
    testSyntaxError(`"use strict"; class C {}; class C2 extends C { async constructor() {} }`);
    testSyntaxError(`"use strict"; class C { static async prototype() {} }`);
    testSyntaxError(`"use strict"; class C {}; class C2 extends C { static async prototype() {} }`);

    testSyntaxError(`"use strict"; var f = async() => ((async(x = await 1) => x)();`);

    // Henrique Ferreiro's bug (tm)
    testSyntaxError(`"use strict"; (async function foo1() { } foo2 => 1)`);
    testSyntaxError(`"use strict"; (async function foo3() { } () => 1)`);
    testSyntaxError(`"use strict"; (async function foo4() { } => 1)`);
    testSyntaxError(`"use strict"; (async function() { } foo5 => 1)`);
    testSyntaxError(`"use strict"; (async function() { } () => 1)`);
    testSyntaxError(`"use strict"; (async function() { } => 1)`);
    testSyntaxError(`"use strict"; (async.foo6 => 1)`);
    testSyntaxError(`"use strict"; (async.foo7 foo8 => 1)`);
    testSyntaxError(`"use strict"; (async.foo9 () => 1)`);
    testSyntaxError(`"use strict"; (async().foo10 => 1)`);
    testSyntaxError(`"use strict"; (async().foo11 foo12 => 1)`);
    testSyntaxError(`"use strict"; (async().foo13 () => 1)`);
    testSyntaxError(`"use strict"; (async['foo14'] => 1)`);
    testSyntaxError(`"use strict"; (async['foo15'] foo16 => 1)`);
    testSyntaxError(`"use strict"; (async['foo17'] () => 1)`);
    testSyntaxError(`"use strict"; (async()['foo18'] => 1)`);
    testSyntaxError(`"use strict"; (async()['foo19'] foo20 => 1)`);
    testSyntaxError(`"use strict"; (async()['foo21'] () => 1)`);
    testSyntaxError('"use strict"; (async`foo22` => 1)');
    testSyntaxError('"use strict"; (async`foo23` foo24 => 1)');
    testSyntaxError('"use strict"; (async`foo25` () => 1)');
    testSyntaxError('"use strict"; (async`foo26`.bar27 => 1)');
    testSyntaxError('"use strict"; (async`foo28`.bar29 foo30 => 1)');
    testSyntaxError('"use strict"; (async`foo31`.bar32 () => 1)');

    // assert that errors are still thrown for calls that may have been async functions
    testSyntaxError(`"use strict"; function async() {}
                     async({ foo33 = 1 })`);

    testSyntaxError(`"use strict"; var O = { async method(eval) {} }`);
    testSyntaxError(`"use strict"; var O = { async ['meth' + 'od'](eval) {} }`);
    testSyntaxError(`"use strict"; var O = { async 'method'(eval) {} }`);
    testSyntaxError(`"use strict"; var O = { async 0(eval) {} }`);

    testSyntaxError(`"use strict"; var O = { async method(arguments) {} }`);
    testSyntaxError(`"use strict"; var O = { async ['meth' + 'od'](arguments) {} }`);
    testSyntaxError(`"use strict"; var O = { async 'method'(arguments) {} }`);
    testSyntaxError(`"use strict"; var O = { async 0(arguments) {} }`);

    testSyntaxError(`"use strict"; var O = { async method(dupe, dupe) {} }`);
})();

(function testAwaitInFormalParameters() {
    var testData = [
        `async function f(await) {}`,
        `async function f(...await) {}`,
        `async function f(await = 1) {}`,
        `async function f([await]) {}`,
        `async function f([await = 1]) {}`,
        `async function f({ await }) {}`,
        `async function f({ await = 1 }) {}`,
        `async function f({ } = await) {}`,

        `(async function(await) {})`,
        `(async function(...await) {})`,
        `(async function(await = 1) {})`,
        `(async function([await]) {})`,
        `(async function([await = 1]) {})`,
        `(async function({ await }) {})`,
        `(async function({ await = 1 }) {})`,
        `(async function({ } = await) {})`,

        `var asyncArrow = async(await) => 1;`,
        `var asyncArrow = async(await) => {};`,
        `var asyncArrow = async(...await) => 1;`,
        `var asyncArrow = async(...await) => {};`,
        `var asyncArrow = async(await = 1) => 1;`,
        `var asyncArrow = async(await = 1) => {};`,
        `var asyncArrow = async([await]) => 1;`,
        `var asyncArrow = async([await]) => {};`,
        `var asyncArrow = async([await = 1]) => 1;`,
        `var asyncArrow = async([await = 1]) => {};`,
        `var asyncArrow = async([] = await) => 1;`,
        `var asyncArrow = async([] = await) => {};`,
        `var asyncArrow = async({ await }) => 1;`,
        `var asyncArrow = async({ await } ) => {};`,
        `var asyncArrow = async({ await = 1}) => 1;`,
        `var asyncArrow = async({ await = 1}) => {};`,
        `var asyncArrow = async({ } = await) => 1;`,
        `var asyncArrow = async({ } = await) => {};`,

        `({ async method(await) {} })`,
        `({ async method(...await) {} })`,
        `({ async method(await = 1) {} })`,
        `({ async method([await]) {} })`,
        `({ async method([await = 1]) {} })`,
        `({ async method({ await }) {} })`,
        `({ async method({ await = 1 }) {} })`,
        `({ async method({ } = await) {} })`,

        `(class { async method(await) {} })`,
        `(class { async method(...await) {} })`,
        `(class { async method(await = 1) {} })`,
        `(class { async method([await]) {} })`,
        `(class { async method([await = 1]) {} })`,
        `(class { async method({ await }) {} })`,
        `(class { async method({ await = 1 }) {} })`,
        `(class { async method({ } = await) {} })`,

        `(class { static async method(await) {} })`,
        `(class { static async method(...await) {} })`,
        `(class { static async method(await = 1) {} })`,
        `(class { static async method([await]) {} })`,
        `(class { static async method([await = 1]) {} })`,
        `(class { static async method({ await }) {} })`,
        `(class { static async method({ await = 1 }) {} })`,
        `(class { static async method({ } = await) {} })`,
    ];

    for (let script of testData) {
        testSyntaxError(script);
        testSyntaxError(`"use strict"; ${script}`);

        var nested = `var await; var f = (async function() { ${script} });`;
        testSyntaxError(nested);
        testSyntaxError(`"use strict"; ${nested}`);
    }
})();

testSyntaxError(`
async function fn(arguments) {
    "use strict";
}
`, `SyntaxError: Invalid parameters or function name in strict mode.`);

testSyntaxError(`
async function fn(eval) {
    "use strict";
}
`, `SyntaxError: Invalid parameters or function name in strict mode.`);

testSyntaxError(`
async function arguments() {
    "use strict";
}
`, `SyntaxError: 'arguments' is not a valid function name in strict mode.`);

testSyntaxError(`
async function eval() {
    "use strict";
}
`, `SyntaxError: 'eval' is not a valid function name in strict mode.`);

testSyntaxError(`
async function fn(a) {
    let a = 1;
}
`, `SyntaxError: Cannot declare a let variable twice: 'a'.`);

testSyntaxError(`
async function fn(b) {
    const b = 1;
}
`, `SyntaxError: Cannot declare a const variable twice: 'b'.`);

(function testMethodDefinition() {
    testSyntax("({ async [foo]() {} })");
    testSyntax("({ async [Symbol.asyncIterator]() {} })");
    testSyntax("({ async 0() {} })");
    testSyntax("({ async 'string'() {} })");
    testSyntax("({ async ident() {} })");

    testSyntax("(class { async [foo]() {} })");
    testSyntax("(class { async [Symbol.asyncIterator]() {} })");
    testSyntax("(class { async 0() {} })");
    testSyntax("(class { async 'string'() {} })");
    testSyntax("(class { async ident() {} })");

    testSyntax("(class { static async [foo]() {} })");
    testSyntax("(class { static async [Symbol.asyncIterator]() {} })");
    testSyntax("(class { static async 0() {} })");
    testSyntax("(class { static async 'string'() {} })");
    testSyntax("(class { static async ident() {} })");
})();

(function testLineTerminator() {
    let testLineFeedErrors = (prefix, suffix) => {
        testSyntaxError(`${prefix}// comment
                         ${suffix}`);
        testSyntaxError(`${prefix}/* comment
                         */ ${suffix}`);
        testSyntaxError(`${prefix}
                         ${suffix}`);
        testSyntaxError(`"use strict";${prefix}// comment
                         ${suffix}`);
        testSyntaxError(`"use strict";${prefix}/* comment
                         */ ${suffix}`);
        testSyntaxError(`"use strict";${prefix}
                         ${suffix}`);
    };

    let testLineFeeds = (prefix, suffix) => {
        testSyntax(`${prefix}// comment
                    ${suffix}`);
        testSyntax(`${prefix}/* comment
                    */${suffix}`);
        testSyntax(`${prefix}
                    ${suffix}`);
        testSyntax(`"use strict";${prefix}// comment
                    ${suffix}`);
        testSyntax(`"use strict";${prefix}/* comment
                    */${suffix}`);
        testSyntax(`"use strict";${prefix}
                    ${suffix}`);
    };

    let tests = [
        // ObjectLiteral AsyncMethodDefinition
        { prefix: "({ async", suffix: "method() {} }).method" },

        // ClassLiteral AsyncMethodDefinition
        { prefix: "(class { async", suffix: "method() {} }).prototype.method" },

        // AsyncArrowFunctions
        { prefix: "(async", suffix: "param => 1)" },
        { prefix: "(async", suffix: "(param) => 1)" },
        { prefix: "(async", suffix: "param => {})" },
        { prefix: "(async", suffix: "(param) => {})" },
    ];

    for (let { prefix, suffix } of tests) {
        testSyntax(`${prefix} ${suffix}`);
        testSyntax(`"use strict";${prefix} ${suffix}`);
        shouldBe("function", typeof eval(`${prefix} ${suffix}`));
        shouldBe("function", typeof eval(`"use strict";${prefix} ${suffix}`));
        testLineFeedErrors(prefix, suffix);
    }

    // AsyncFunctionDeclaration
    testSyntax("async function foo() {}");
    testLineFeeds("async", "function foo() {}");

    // AsyncFunctionExpression
    testSyntax("var x = async function foo() {}");
    testSyntax("'use strict';var x = async function foo() {}");
    testLineFeeds("var x = async", "function foo() {}");
    testLineFeedErrors("var x = async", "function() {}");
})();
