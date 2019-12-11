/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

const _fail = (msg, extra) => {
    throw new Error(msg + (extra ? ": " + extra : ""));
};

const assert = {};

const isNotA = assert.isNotA = (v, t, msg) => {
    if (typeof v === t)
        _fail(`Shouldn't be ${t}`, msg);
};

const isA = assert.isA = (v, t, msg) => {
    if (typeof v !== t)
        _fail(`Should be ${t}, got ${typeof(v)}`, msg);
};

const isNotUndef = assert.isNotUndef = (v, msg) => isNotA(v, "undefined", msg);
assert.isUndef = (v, msg) => isA(v, "undefined", msg);
assert.notObject = (v, msg) => isNotA(v, "object", msg);
const isObject = assert.isObject = (v, msg) => isA(v, "object", msg);
assert.notString = (v, msg) => isNotA(v, "string", msg);
assert.isString = (v, msg) => isA(v, "string", msg);
assert.notNumber = (v, msg) => isNotA(v, "number", msg);
assert.isNumber = (v, msg) => isA(v, "number", msg);
assert.notFunction = (v, msg) => isNotA(v, "function", msg);
assert.isFunction = (v, msg) => isA(v, "function", msg);

assert.hasObjectProperty = (o, p, msg) => {
    isObject(o, msg);
    isNotUndef(o[p], msg, `expected object to have property ${p}`);
};

assert.isArray = (v, msg) => {
    if (!Array.isArray(v))
        _fail(`Expected an array, got ${typeof(v)}`, msg);
};

assert.isNotArray = (v, msg) => {
    if (Array.isArray(v))
        _fail(`Expected to not be an array`, msg);
};

assert.truthy = (v, msg) => {
    if (!v)
        _fail(`Expected truthy`, msg);
};

assert.falsy = (v, msg) => {
    if (v)
        _fail(`Expected falsy`, msg);
};

assert.eq = (lhs, rhs, msg) => {
    if (typeof lhs !== typeof rhs)
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    if (Array.isArray(lhs) && Array.isArray(rhs) && (lhs.length === rhs.length)) {
        for (let i = 0; i !== lhs.length; ++i)
            eq(lhs[i], rhs[i], msg);
    } else if (lhs !== rhs) {
        if (typeof lhs === "number" && isNaN(lhs) && isNaN(rhs))
            return;
        _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    } else {
        if (typeof lhs === "number" && (1.0 / lhs !== 1.0 / rhs)) // Distinguish -0.0 from 0.0.
            _fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
    }
};

const canonicalizeI32 = (number) => {
    if (Math.round(number) === number && number >= 2 ** 31)
        number = number - 2 ** 32;
    return number;
}

assert.eqI32 = (lhs, rhs, msg) => {
    return eq(canonicalizeI32(lhs), canonicalizeI32(rhs), msg);
};

assert.ge = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs >= rhs))
        _fail(`Expected: "${lhs}" < "${rhs}"`, msg);
};

assert.le = (lhs, rhs, msg) => {
    isNotUndef(lhs);
    isNotUndef(rhs);
    if (!(lhs <= rhs))
        _fail(`Expected: "${lhs}" > "${rhs}"`, msg);
};

const _throws = (func, type, message, ...args) => {
    try {
        func(...args);
    } catch (e) {
        if (e instanceof type) {
            if (e.message === message)
                return e;
            // Ignore source information at the end of the error message if the
            // expected message didn't specify that information. Sometimes it
            // changes, or it's tricky to get just right.
            const evaluatingIndex = e.message.indexOf(" (evaluating '");
            if (evaluatingIndex !== -1) {
                const cleanMessage = e.message.substring(0, evaluatingIndex);
                if (cleanMessage === message)
                    return e;
            }
        }
        _fail(`Expected to throw a ${type.name} with message "${message}", got ${e.name} with message "${e.message}"`);
    }
    _fail(`Expected to throw a ${type.name} with message "${message}"`);
};

const _instanceof = (obj, type, msg) => {
    if (!(obj instanceof type))
        _fail(`Expected a ${typeof(type)}, got ${typeof obj}`);
};

// Use underscore names to avoid clashing with builtin names.
assert.throws = _throws;
assert.instanceof = _instanceof;

const asyncTestImpl = (promise, thenFunc, catchFunc) => {
    asyncTestStart(1);
    promise.then(thenFunc).catch(catchFunc);
};

const printExn = (e) => {
    print("Failed: ", e);
    print(e.stack);
};

assert.asyncTest = (promise) => asyncTestImpl(promise, asyncTestPassed, printExn);
assert.asyncTestEq = (promise, expected) => {
    const thenCheck = (value) => {
        if (value === expected)
            return asyncTestPassed();
        print("Failed: got ", value, " but expected ", expected);

    }
    asyncTestImpl(promise, thenCheck, printExn);
};

// ------ end assert

onmessage = function({ data: module }) {
    WebAssembly.instantiate(module).then(($1) => {
        try {
            assert.eq($1.exports.tbl_size(), 20)
            assert.eq($1.exports.tbl_grow(5), 20)
            assert.eq($1.exports.tbl_size(), 25)
            assert.eq($1.exports.tbl.get(0), null)
            assert.eq($1.exports.tbl.get(24), $1.exports.tbl_size)

            for (let i=0; i<1000; ++i) {
                $1.exports.tbl_fill(1,"hi",3)
                assert.eq($1.exports.tbl.get(0), null)
                assert.eq($1.exports.tbl.get(1), "hi")
                assert.eq($1.exports.tbl.get(2), "hi")
                assert.eq($1.exports.tbl.get(3), "hi")
                assert.eq($1.exports.tbl.get(4), null)

                assert.eq($1.exports.make_ref(), $1.exports.tbl_grow)
            }
        } catch (e) {
            console.log(e)
            postMessage("" + e)
            return
        }

        console.log("Good result");
        postMessage("done");
    }, (e) => postMessage("" + e))
}
