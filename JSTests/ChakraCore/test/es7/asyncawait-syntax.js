//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// ES6 Async Await tests -- verifies syntax of async/await

WScript.LoadScriptFile("..\\UnitTestFramework\\UnitTestFramework.js");

var tests = [
    {
        name: "Async and Await keyword as identifier",
        body: function () {
            var async = [2, 3, 4];
            var await = 3;
            var o = { async };
            o.async = 0;

            assert.areEqual(2, async[0], "async[0] === 2");
            assert.areEqual(3, await, "await === 3");
            assert.areEqual(0, o.async, "o.async === 0");
        }
    },
    {
        name: "Await keyword as identifier",
        body: function () {
            function method() {
                var await = 1;
                return await;
            }
            function await() {
                return 2;
            }

            assert.areEqual(1, method(), "method() === 1");
            assert.areEqual(2, await(), "await() === 2");

            assert.throws(function () { eval("async function method() { var await = 1; }"); }, SyntaxError, "'await' cannot be used as an identifier in an async function.", "The use of a keyword for an identifier is invalid");
            assert.throws(function () { eval("async function method(await;) { }"); }, SyntaxError, "'await' cannot be used as an identifier in an async function.", "The use of a keyword for an identifier is invalid");
            assert.throws(function () { eval("async function method() { var x = await; }"); }, SyntaxError, "'await' cannot be used as an identifier in an async function.", "Syntax error");
        }
    },
    {
        name: "Async keyword as generator",
        body: function () {
            assert.throws(function () { eval("async function* badFunction() { }"); }, SyntaxError, "'async' keyword is not allowed with a generator in a statement", "Syntax error");
            assert.throws(function () { eval("var badVariable = async function*() { }"); }, SyntaxError, "'async' keyword is not allowed with a generator in an expression", "Syntax error");
            assert.throws(function () { eval("var o { async *badFunction() { } };"); }, SyntaxError, "'async' keyword is not allowed with a generator in a object literal member", "Expected ';'");
            assert.throws(function () { eval("class C { async *badFunction() { } };"); }, SyntaxError, "'async' keyword is not allowed with a generator in a class member", "Syntax error");
        }
    },
    {
        name: "Async classes",
        body: function () {
            assert.throws(function () { eval("class A { async constructor() {} }"); }, SyntaxError, "'async' keyword is not allowed with a constructor", "Syntax error");
            assert.throws(function () { eval("class A { async get foo() {} }"); }, SyntaxError, "'async' keyword is not allowed with a getter", "Syntax error");
            assert.throws(function () { eval("class A { async set foo() {} }"); }, SyntaxError, "'async' keyword is not allowed with a setter", "Syntax error");
            assert.throws(function () { eval("class A { async static staticAsyncMethod() {} }"); }, SyntaxError, "'async' keyword is not allowed before a static keyword in a function declaration", "Expected '('");
            assert.throws(function () { eval("class A { static async prototype() {} }"); }, SyntaxError, "static async method cannot be named 'prototype'", "Syntax error");
        }
    },
    {
        name: "Await in eval global scope",
        body: function () {
            assert.throws(function () { eval("var result = await call();"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("await call();"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");

            assert.throws(function () { eval("await a;"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("await a[0];"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("await o.p;"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("a[await p];"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ']'");
            assert.throws(function () { eval("a + await p;"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("await p + await q;"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ';'");
            assert.throws(function () { eval("foo(await p, await q);"); }, SyntaxError, "'await' keyword is not allowed in eval global scope", "Expected ')'");

            assert.throws(function () { eval("var lambdaParenNoArg = await () => x < y;"); }, SyntaxError, "'await' keyword is not allowed with a non-async lambda expression", "Syntax error");
            assert.throws(function () { eval("var lambdaArgs = await async (a, b ,c) => a + b + c;"); }, SyntaxError, "There miss parenthises", "Expected ';'");
            assert.throws(function () { eval("var lambdaArgs = await (async (a, b ,c) => a + b + c);"); }, ReferenceError, "The 'await' function doesn't exists in this scope", "'await' is undefined");
        }
    },
    {
        name: "Await in a non-async function",
        body: function () {
            assert.throws(function () { eval("function method() { var x = await call(); }"); }, SyntaxError, "'await' cannot be used in a non-async function.", "Expected ';'");
        }
    },
    {
        name: "Await in strict mode",
        body: function () {
            "strict mode";
            assert.doesNotThrow(function () { eval("function f() { var await; }"); }, "Can name var variable 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { let await; }"); }, "Can name let variable 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { const await = 10; }"); }, "Can name const variable 'await' in non-generator body");

            assert.doesNotThrow(function () { eval("function f() { function await() { } }"); }, "Can name function 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { function* await() { } }"); }, "Can name generator function 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { var fe = function await() { } }"); }, "Can name function expression 'await' in non-generator body");

            assert.doesNotThrow(function () { eval("function f() { class await { } }"); }, "Can name class 'await' in non-generator body");

            assert.doesNotThrow(function () { eval("function f() { var o = { await: 10 } }"); }, "Can name object literal property 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { var o = { get await() { } } }"); }, "Can name accessor method 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { var o = { await() { } } }"); }, "Can name concise method 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { var o = { *await() { } } }"); }, "Can name generator concise method 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { var await = 10; var o = { await }; }"); }, "Can name shorthand property 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { class C { await() { } } }"); }, "Can name method 'await' in non-generator body");
            assert.doesNotThrow(function () { eval("function f() { class C { *await() { } } }"); }, "Can name generator method 'await' in non-generator body");
        }
    },
    {
        name: "Async function is not a constructor",
        body: function () {
            async function foo() { }

            assert.isFalse(foo.hasOwnProperty('prototype'), "An async function does not have a property named 'prototype'.");

            assert.throws(function () { eval("new foo();"); }, TypeError, "An async function cannot be instantiated because it is not have a constructor.", "Function is not a constructor");
        }
    },
    {
        name: "async lambda parsing",
        body: function () {
            var a = async => async;
            assert.areEqual(42, a(42), "async used as single parameter name for arrow function still works with async feature turned on");

            var b = async () => { };
            var c = async x => x;
            var d = async (a, b) => { };
        }
    },
    {
        name: "It is a Syntax Error if FormalParameters Contains AwaitExpression is true",
        body: function () {
            assert.throws(function () { eval("async function af(a, b = await a) { }"); }, SyntaxError, "await expressions not allowed in non-strict async function", "'await' expression not allowed in this context");
            assert.throws(function () { eval("async function af(a, b = await a) { 'use strict'; }"); }, SyntaxError, "await expressions not allowed in self-strict async function", "'await' expression not allowed in this context");
            assert.throws(function () { "use strict"; eval("async function af(a, b = await a) { }"); }, SyntaxError, "await expressions not allowed in parent-strict async function", "'await' expression not allowed in this context");

            assert.doesNotThrow(function () { eval("function f(a = async function (x) { await x; }) { a(); } f();"); }, "await is allowed within the body of an async function that appears in a default parameter value expression");

            assert.throws(function () { eval("async function af(x) { function f(a = await x) { } f(); } af();"); }, SyntaxError, "await expression is not available within non-async function parameter default expression", "Expected ')'");
        }
    },
    {
        name: "[no LineTerminator here] after `async` in grammar",
        body: function () {
            assert.throws(function () { eval("async\nfunction af() { }"); }, ReferenceError, "AsyncFunctionDeclaration", "'async' is undefined");
            assert.throws(function () { eval("var af = async\nfunction () { }"); }, SyntaxError, "AsyncFunctionExpression", "Expected identifier");
            assert.throws(function () { eval("var o = { async\nam() { } };"); }, SyntaxError, "AsyncMethod in object literal", "Expected ':'");
            assert.throws(function () { eval("class C { async\nam() { } };"); }, SyntaxError, "AsyncMethod in class", "Expected '('");
            assert.throws(function () { eval("var aaf = async\n(x, y) => { };"); }, SyntaxError, "AsyncArrowFunction", "Syntax error");
        }
    },
    {
        name: "'arguments' and 'eval' are not allowed as formal parameter names in strict mode",
        body: function () {
            assert.doesNotThrow(function () { eval("async function af(arguments) { }"); }, "'arguments' can be the name of a parameter in a non-strict mode async function");
            assert.doesNotThrow(function () { eval("async function af(eval) { }"); }, "'eval' can be the name of a parameter in a non-strict mode async function");

            assert.throws(function () { eval("async function af(arguments) { 'use strict'; }"); }, SyntaxError, "'arguments' cannot be the name of a parameter in an async function that turns on strict mode", "Invalid usage of 'arguments' in strict mode");
            assert.throws(function () { eval("async function af(eval) { 'use strict'; }"); }, SyntaxError, "'eval' cannot be the name of a parameter in an async function that turns on strict mode", "Invalid usage of 'eval' in strict mode");

            assert.throws(function () { "use strict"; eval("async function af(arguments) { }"); }, SyntaxError, "'arguments' cannot be the name of a parameter in an async function that is already in strict mode", "Invalid usage of 'arguments' in strict mode");
            assert.throws(function () { "use strict"; eval("async function af(eval) { }"); }, SyntaxError, "'eval' cannot be the name of a parameter in an async function that is already in strict mode", "Invalid usage of 'eval' in strict mode");
        }
    },
    {
        name: "duplicate formal parameter names are not allowed in strict mode",
        body: function () {
            assert.doesNotThrow(function () { eval("async function af(x, x) { }"); }, "duplicate parameter names are allowed in a non-strict mode async function");
            assert.doesNotThrow(function () { eval("async function af(a, b, a) { }"); }, "duplicate parameter names are allowed in a non-strict mode async function (when there are other names)");

            assert.throws(function () { eval("async (x, x) => { }"); }, SyntaxError, "duplicate parameter names are not allowed in a non-strict mode async arrow function due to arrow function static semantics", "Duplicate formal parameter names not allowed in this context");
            assert.throws(function () { eval("async (a, b, a) => { }"); }, SyntaxError, "duplicate parameter names are not allowed in a non-strict mode async arrow function due to arrow function static semantics (when there are other names)", "Duplicate formal parameter names not allowed in this context");

            assert.throws(function () { eval("async function af(x, x) { 'use strict'; }"); }, SyntaxError, "duplicate parameter names are not allowed in an async function that turns on strict mode", "Duplicate formal parameter names not allowed in strict mode");
            assert.throws(function () { eval("async function af(a, b, a) { 'use strict'; }"); }, SyntaxError, "duplicate parameter names are not allowed in an async function that turns on strict mode (when there are other names)", "Duplicate formal parameter names not allowed in strict mode");

            assert.throws(function () { "use strict"; eval("async function af(x, x) { }"); }, SyntaxError, "duplicate parameter names are not allowed in an async function that is already in strict mode", "Duplicate formal parameter names not allowed in strict mode");
            assert.throws(function () { "use strict"; eval("async function af(a, b, a) { }"); }, SyntaxError, "duplicate parameter names are not allowed in an async function that is already in strict mode (when there are other names)", "Duplicate formal parameter names not allowed in strict mode");
        }
    },
    {
        name: "local variables with same names as formal parameters have proper redeclaration semantics",
        body: function () {
            assert.doesNotThrow(function () { eval("async function af(x) { var x; }"); }, "var with same name as formal is not an error");
            assert.throws(function () { eval("async function af(x) { let x; }"); }, SyntaxError, "let with same name as formal is an error", "Let/Const redeclaration");
            assert.throws(function () { eval("async function af(x) { const x = 1; }"); }, SyntaxError, "const with same name as formal is an error", "Let/Const redeclaration");
            assert.doesNotThrow(function () { eval("async function af(x) { function x() { } }"); }, "local function with same name as formal is not an error");
            assert.throws(function () { eval("async function af(x) { class x { } }"); }, SyntaxError, "class with same name as formal is an error", "Let/Const redeclaration");
        }
    },
];

testRunner.runTests(tests, { verbose: WScript.Arguments[0] != "summary" });
