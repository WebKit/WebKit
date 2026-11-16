const existingKeys = ["cocoa", "test", Symbol.toStringTag];
const nonExistingKeys = ["foo", Symbol("foo")];
const allKeys = [...existingKeys, ...nonExistingKeys];
const testValue = Object.freeze({});

function assert(x, key) {
    if (!x)
        throw new Error(`Bad assertion! Key: ${String(key)}`);
}

function shouldThrow(func, errorMessage) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (String(error) !== errorMessage)
            throw new Error(`Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error("Didn't throw!");
}

function testDirectSet(ns) {
    for (const key of allKeys) {
        ns[key] = testValue;
        assert(ns[key] !== testValue, key);

        shouldThrow(() => { "use strict"; ns[key] = testValue; }, "TypeError: Attempted to assign to readonly property.");
        assert(ns[key] !== testValue, key);
    }
}

function testPrototypeChainSet(ns) {
    for (const key of allKeys) {
        const heir = Object.create(ns);

        heir[key] = testValue;
        assert(Object.getOwnPropertyDescriptor(heir, key) === undefined, key);

        shouldThrow(() => { "use strict"; heir[key] = testValue; }, "TypeError: Attempted to assign to readonly property.");
        assert(Object.getOwnPropertyDescriptor(heir, key) === undefined, key);
    }
}

function testReflectSet(ns) {
    "use strict";

    for (const existingKey of existingKeys) {
        assert(!Reflect.set(ns, existingKey, testValue), existingKey);

        const target = {};
        assert(!Reflect.set(target, existingKey, testValue, ns), existingKey);

        const isWritable = existingKey !== Symbol.toStringTag;
        assert(Reflect.set(target, existingKey, ns[existingKey], ns) === isWritable, existingKey);

        assert(ns[existingKey] !== testValue, existingKey);
        assert(target[existingKey] === undefined, existingKey);
    }

    for (const nonExistingKey of nonExistingKeys) {
        assert(!Reflect.set(ns, nonExistingKey, testValue), nonExistingKey);
        assert(ns[nonExistingKey] === undefined, nonExistingKey);

        const target = {};
        assert(!Reflect.set(target, nonExistingKey, testValue, ns), nonExistingKey);
        assert(target[nonExistingKey] === undefined, nonExistingKey);
    }

    for (const key of allKeys) {
        const target = Object.create(ns);
        const receiver = {};

        assert(!Reflect.set(target, key, testValue, receiver), key);
        assert(Object.getOwnPropertyDescriptor(target, key) === undefined, key);
        assert(Object.getOwnPropertyDescriptor(receiver, key) === undefined, key);
    }
}

import("./resources/module-namespace-access.js").then(ns => {
    testDirectSet(ns);
    testPrototypeChainSet(ns);
    testReflectSet(ns);
}).catch(err => {
    print(`!!! ${err}\n${err.stack}`);
    $vm.abort();
});

drainMicrotasks();
