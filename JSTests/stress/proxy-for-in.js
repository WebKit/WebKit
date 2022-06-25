// Author:  Caitlin Potter  <caitp@igalia.com>
"use strict";

function assert(b, m = "Bad assertion") {
    if (!b)
        throw new Error(m);
}

assert.eq = function eq(expected, actual, m = `Expected «${actual}» to be «${expected}».`) {
    assert(expected === actual, m); 
}

assert.arrEq = function arrEq(expected, actual, m = `Expected «${actual}» to be «${expected}».`) {
    assert(Array.isArray(expected));
    assert(Array.isArray(actual), `Expected «${actual}» to be an Array.`);
    assert.eq(expected.length, actual.length, `Expected «${actual}» to have ${expected.length} items`);
    for (let i = 0; i < expected.length; ++i) {
        let e = expected[i], a = actual[i];
        if (Array.isArray(e))
            assert.arrEq(e, a);
        else
            assert.eq(expected[i], actual[i]);
    }
}

// Enumerable, no ownKeys trap
{
    let log = [];
    let p = new Proxy(Object.create(null, {
        x: {
            enumerable: true,
            configurable: true,
            value: 0
        },
    }), {
        getOwnPropertyDescriptor(target, pname) {
            log.push(`gopd ${pname}`);
            return Reflect.getOwnPropertyDescriptor(target, pname);
        }
    });
    for (let k in p) log.push(`enumerate ${k}`);

    assert.arrEq([
        "gopd x",
        "enumerate x",
    ], log);
}

// Non-enumerable, no ownKeys trap
{
    let log = [];
    let p = new Proxy(Object.create(null, {
        x: {
            enumerable: false,
            configurable: true,
            value: 0
        },
    }), {
        getOwnPropertyDescriptor(target, pname) {
            log.push(`gopd ${pname}`);
            return Reflect.getOwnPropertyDescriptor(target, pname);
        }
    });
    for (let k in p) log.push(`enumerate ${k}`);

    assert.arrEq([
        "gopd x",
    ], log);
}

// Enumerable, with ownKeys trap
{
    let log = [];
    let p = new Proxy(Object.create(null, {
        x: {
            enumerable: true,
            configurable: true,
            value: 0
        },
    }), {
        getOwnPropertyDescriptor(target, pname) {
            log.push(`gopd ${pname}`);
            return Reflect.getOwnPropertyDescriptor(target, pname);
        },
        ownKeys(target) {
            return Reflect.ownKeys(target);
        }
    });
    for (let k in p) log.push(`enumerate ${k}`);

    assert.arrEq([
        "gopd x",
        "enumerate x",
    ], log);
}

// Non-enumerable, with ownKeys trap
{
    let log = [];
    let p = new Proxy(Object.create(null, {
        x: {
            enumerable: false,
            configurable: true,
            value: 0
        },
    }), {
        getOwnPropertyDescriptor(target, pname) {
            log.push(`gopd ${pname}`);
            return Reflect.getOwnPropertyDescriptor(target, pname);
        },
        ownKeys(target) {
            return Reflect.ownKeys(target);
        }
    });
    for (let k in p) log.push(`enumerate ${k}`);

    assert.arrEq([
        "gopd x",
    ], log);
}
