// Copyright (C) 2024 Mozilla Corporation. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
includes: [sm/non262.js, sm/non262-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var ieval = eval;
var AsyncFunction = async function(){}.constructor;

var functionContext = {
    Function: {
        constructor: Function,
        toSourceBody: code => `function f() { ${code} }`,
        toSourceParameter: code => `function f(x = ${code}) { }`,
    },
    AsyncFunction: {
        constructor: AsyncFunction,
        toSourceBody: code => `async function f() { ${code} }`,
        toSourceParameter: code => `async function f(x = ${code}) { }`,
    },
};

function assertSyntaxError(kind, code) {
    var {constructor, toSourceBody, toSourceParameter} = functionContext[kind];
    var body = toSourceBody(code);
    var parameter = toSourceParameter(code);

    assertThrowsInstanceOf(() => { constructor(code); }, SyntaxError, constructor.name + ":" + code);
    assertThrowsInstanceOf(() => { constructor(`x = ${code}`, ""); }, SyntaxError, constructor.name + ":" + code);

    assertThrowsInstanceOf(() => { eval(body); }, SyntaxError, "eval:" + body);
    assertThrowsInstanceOf(() => { ieval(body); }, SyntaxError, "indirect eval:" + body);

    assertThrowsInstanceOf(() => { eval(parameter); }, SyntaxError, "eval:" + parameter);
    assertThrowsInstanceOf(() => { ieval(parameter); }, SyntaxError, "indirect eval:" + parameter);
}

function assertNoSyntaxError(kind, code) {
    var {constructor, toSourceBody, toSourceParameter} = functionContext[kind];
    var body = toSourceBody(code);
    var parameter = toSourceParameter(code);

    constructor(code);
    constructor(`x = ${code}`, "");

    eval(body);
    ieval(body);

    eval(parameter);
    ieval(parameter);
}

function assertSyntaxErrorAsync(code) {
    assertNoSyntaxError("Function", code);
    assertSyntaxError("AsyncFunction", code);
}

function assertSyntaxErrorBoth(code) {
    assertSyntaxError("Function", code);
    assertSyntaxError("AsyncFunction", code);
}


// Bug 1353691
// |await| expression is invalid in arrow functions in async-context.
// |await/r/g| first parses as |AwaitExpression RegularExpressionLiteral|, when reparsing the
// arrow function, it is parsed as |IdentRef DIV IdentRef DIV IdentRef|. We need to ensure in this
// case, that we still treat |await| as a keyword and hence throw a SyntaxError.
assertSyntaxErrorAsync("(a = await/r/g) => {}");
assertSyntaxErrorBoth("async(a = await/r/g) => {}");

// Also applies when nesting arrow functions.
assertSyntaxErrorAsync("(a = (b = await/r/g) => {}) => {}");
assertSyntaxErrorBoth("async(a = (b = await/r/g) => {}) => {}");
assertSyntaxErrorBoth("(a = async(b = await/r/g) => {}) => {}");
assertSyntaxErrorBoth("async(a = async(b = await/r/g) => {}) => {}");


// Bug 1355860
// |await| cannot be used as rest-binding parameter in arrow functions in async-context.
assertSyntaxErrorAsync("(...await) => {}");
assertSyntaxErrorBoth("async(...await) => {}");

assertSyntaxErrorAsync("(a, ...await) => {}");
assertSyntaxErrorBoth("async(a, ...await) => {}");

// Also test nested arrow functions.
assertSyntaxErrorAsync("(a = (...await) => {}) => {}");
assertSyntaxErrorBoth("(a = async(...await) => {}) => {}");
assertSyntaxErrorBoth("async(a = (...await) => {}) => {}");
assertSyntaxErrorBoth("async(a = async(...await) => {}) => {}");

assertSyntaxErrorAsync("(a = (b, ...await) => {}) => {}");
assertSyntaxErrorBoth("(a = async(b, ...await) => {}) => {}");
assertSyntaxErrorBoth("async(a = (b, ...await) => {}) => {}");
assertSyntaxErrorBoth("async(a = async(b, ...await) => {}) => {}");


