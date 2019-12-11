/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://moz.org/MPL/2.0/. */

function shouldBe(expected, actual, msg = "") {
    if (msg)
        msg = " for " + msg;
    if (actual !== expected)
        throw new Error("bad value" + msg + ": " + actual + ". Expected " + expected);
}

function shouldBeAsync(expected, run, msg) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (hadError)
        throw actual;
    shouldBe(expected, actual, msg);
}

function shouldThrow(run, errorType, message) {
    let actual;
    var hadError = false;
    try {
        actual = run();
    } catch (e) {
        hadError = true;
        actual = e;
    }
    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expeced " + run + "() to throw " + errorType.name + " , but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

function shouldThrowAsync(run, errorType, message) {
    let actual;
    var hadError = false;
    run().then(function(value) { actual = value; },
               function(error) { hadError = true; actual = error; });
    drainMicrotasks();
    if (!hadError)
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but did not throw.");
    if (!(actual instanceof errorType))
        throw new Error("Expected " + run + "() to throw " + errorType.name + ", but threw '" + actual + "'");
    if (message !== void 0 && actual.message !== message)
        throw new Error("Expected " + run + "() to throw '" + message + "', but threw '" + actual.message + "'");
}

function assert(cond, msg = "") {
    if (!cond)
        throw new Error(msg);
}

function shouldThrowSyntaxError(str, message) {
    var hadError = false;
    try {
        eval(str);
    } catch (e) {
        if (e instanceof SyntaxError) {
            hadError = true;
            if (typeof message === "string")
                assert(e.message === message, "Expected '" + message + "' but threw '" + e.message + "'");
        }
    }
    assert(hadError, "Did not throw syntax error");
}

// semantics.js
(function mozSemantics() {

async function empty() {
}

async function simpleReturn() {
    return 1;
}

async function simpleAwait() {
    var result = await 2;
    return result;
}

async function simpleAwaitAsync() {
    var result = await simpleReturn();
    return 2 + result;
}

async function returnOtherAsync() {
    return 1 + await simpleAwaitAsync();
}

async function simpleThrower() {
    throw new Error();
}

async function delegatedThrower() {
    var val = await simpleThrower();
    return val;
}

async function tryCatch() {
    try {
        await delegatedThrower();
        return 'FAILED';
    } catch (_) {
        return 5;
    }
}

async function tryCatchThrow() {
    try {
        await delegatedThrower();
        return 'FAILED';
    } catch (_) {
        return delegatedThrower();
    }
}

async function wellFinally() {
    try {
        await delegatedThrower();
    } catch (_) {
        return 'FAILED';
    } finally {
        return 6;
    }
}

async function finallyMayFail() {
    try {
        await delegatedThrower();
    } catch (_) {
        return 5;
    } finally {
        return delegatedThrower();
    }
}

async function embedded() {
    async function inner() {
        return 7;
    }
    return await inner();
}

// recursion, it works!
async function fib(n) {
    return (n == 0 || n == 1) ? n : await fib(n - 1) + await fib(n - 2);
}

// mutual recursion
async function isOdd(n) {
    async function isEven(n) {
        return n === 0 || await isOdd(n - 1);
    }
    return n !== 0 && await isEven(n - 1);
}

// recursion, take three!
var hardcoreFib = async function fib2(n) {
    return (n == 0 || n == 1) ? n : await fib2(n - 1) + await fib2(n - 2);
}

var asyncExpr = async function() {
    return 10;
}

var namedAsyncExpr = async function simple() {
    return 11;
}

async function executionOrder() {
    var value = 0;
    async function first() {
        return (value = value === 0 ? 1 : value);
    }
    async function second() {
        return (value = value === 0 ? 2 : value);
    }
    async function third() {
        return (value = value === 0 ? 3 : value);
    }
    return await first() + await second() + await third() + 6;
}

async function miscellaneous() {
    if (arguments.length === 3 &&
        arguments.callee.name === "miscellaneous")
        return 14;
}

function thrower() {
    throw 15;
}

async function defaultArgs(arg = thrower()) {
}

// Async functions are not constructible
shouldThrow(() => {
    async function Person() {
    }
    new Person();
}, TypeError);

shouldBeAsync(undefined, empty);
shouldBeAsync(1, simpleReturn);
shouldBeAsync(2, simpleAwait);
shouldBeAsync(3, simpleAwaitAsync);
shouldBeAsync(4, returnOtherAsync);
shouldThrowAsync(simpleThrower, Error);
shouldBeAsync(5, tryCatch);
shouldBeAsync(6, wellFinally);
shouldThrowAsync(finallyMayFail, Error);
shouldBeAsync(7, embedded);
shouldBeAsync(8, () => fib(6));
shouldBeAsync(9, executionOrder);
shouldBeAsync(10, asyncExpr);
shouldBeAsync(11, namedAsyncExpr);
shouldBeAsync(12, () => isOdd(12).then(v => v ? "oops" : 12));
shouldBeAsync(13, () => hardcoreFib(7));
shouldBeAsync(14, () => miscellaneous(1, 2, 3));
shouldBeAsync(15, () => defaultArgs().catch(e => e));

})();

// methods.js
(function mozMethods() {

class X {
    constructor() {
        this.value = 42;
    }
    async getValue() {
        return this.value;
    }
    setValue(value) {
        this.value = value;
    }
    async increment() {
        var value = await this.getValue();
        this.setValue(value + 1);
        return this.getValue();
    }
    async getBaseClassName() {
        return 'X';
    }
    static async getStaticValue() {
        return 44;
    }
}

class Y extends X {
    async getBaseClassName() {
        return super.getBaseClassName();
    }
}

var objLiteral = {
    async get() {
        return 45;
    },
    someStuff: 5
};

var x = new X();
var y = new Y();

shouldBeAsync(42, () => x.getValue());
shouldBeAsync(43, () => x.increment());
shouldBeAsync(44, () => X.getStaticValue());
shouldBeAsync(45, () => objLiteral.get());
shouldBeAsync('X', () => y.getBaseClassName());

})();

(function mozFunctionNameInferrence() {

async function test() { }

var anon = async function() { }

shouldBe("test", test.name);
shouldBe("anon", anon.name);

})();

(function mozSyntaxErrors() {

shouldThrowSyntaxError("'use strict'; async function eval() {}");
shouldThrowSyntaxError("'use strict'; async function arguments() {}");
shouldThrowSyntaxError("async function a(k = super.prop) { }");
shouldThrowSyntaxError("async function a() { super.prop(); }");
shouldThrowSyntaxError("async function a() { super(); }");
shouldThrowSyntaxError("async function a(k = await 3) {}");

})();
