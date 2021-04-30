const TEST_VALUE = 1062630713; // Array's "length" is required to be int32

testSetResult([1, 2], "length", true);
testSetResult(
    Object.defineProperty(new Array, "length", { writable: false }),
    "length",
    false,
);

testSetResult(new String, "length", false);
testSetResult(new RegExp, "lastIndex", true);
testSetResult(
    Object.defineProperty(/(?:)/g, "lastIndex", { writable: false }),
    "lastIndex",
    false,
);

for (const key of ["line", "column", "sourceURL", "stack"])
    testSetResult(new Error, key, true);

const mappedArguments = (function(a, b) { return arguments; })(1, 2);
testSetResult(mappedArguments, "callee", true);
testSetResult(mappedArguments, "length", true);
testSetResult(mappedArguments, Symbol.iterator, true);

const unmappedArguments = (function() { "use strict"; return arguments; })();
testSetResult(unmappedArguments, "length", true);
testSetResult(unmappedArguments, Symbol.iterator, true);

var foo;
function bar() {}
for (const key of ["foo", "bar", "parseInt"])
    testSetResult(globalThis, key, true);

for (const key of ["Infinity", "NaN", "undefined"])
    testSetResult(globalThis, key, false);

for (const key of ["name", "length"]) {
    testSetResult(function() {}.bind(), key, false);
    testSetResult(
        Object.defineProperty(function() {}, key, { writable: true }),
        key,
        true,
    );

    testSetResult(() => { "use strict"; }, key, false);
    testSetResult(
        Object.defineProperty(parseFloat, key, { writable: true }),
        key,
        true,
    );
}

testSetResult(function() { "use strict"; }, "prototype", true);
testSetResult(
    Object.defineProperty(function() {}, "prototype", { writable: false }),
    "prototype",
    false,
);

testSetResult(function() {}, "arguments", false);
testSetResult(function() {}, "caller", false);

// === harness ===

function testSetResult(target, key, expectedResult) {
    let cleanUp = putSetterOnPrototype(target, key, () => {});

    for (const primitive of [true, 1, "foo", Symbol(), 0n]) {
        const primitivePrototype = Object.getPrototypeOf(primitive);
        Object.setPrototypeOf(primitivePrototype, target);
        testSetResultInternal(target, key, primitive, false);
        Object.setPrototypeOf(primitivePrototype, Object.prototype);
    }

    cleanUp();
    cleanUp = putSetterOnPrototype(target, key, () => {
        throw new Error(`${String(key)} setter should be unreachable!`);
    });

    for (const receiver of [
        Object.create(target),
        Object.create(Object.create(target)),
        new Proxy(target, {}),
        new Proxy(target, Reflect),
    ])
        testSetResultInternal(target, key, receiver, expectedResult);

    cleanUp();
}

function putSetterOnPrototype(target, key, set) {
    const descriptor = { set, configurable: true };
    const currentPrototype = Object.getPrototypeOf(target);
    const newPrototype = Object.create(null, { [key]: descriptor });

    if (Reflect.setPrototypeOf(target, newPrototype))
        return () => { Object.setPrototypeOf(target, currentPrototype); };

    if (currentPrototype !== null && Reflect.defineProperty(currentPrototype, key, descriptor))
        return () => { "use strict"; delete currentPrototype[key]; };

    throw new Error("This is unreachable: at least one approach should succeed!");
}

function testSetResultInternal(target, key, receiver, expectedResult) {
    receiver[key] = TEST_VALUE;
    shouldBe(receiver[key] === TEST_VALUE, expectedResult, key);

    testSetResultStrict(receiver, key, expectedResult);

    if (receiver === Object(receiver))
        shouldBe(Reflect.set(receiver, key, TEST_VALUE), expectedResult, key);
    shouldBe(Reflect.set(target, key, TEST_VALUE, receiver), expectedResult, key);
}

function testSetResultStrict(receiver, key, expectedResult) {
    "use strict";
    if (expectedResult) {
        receiver[key] = TEST_VALUE;
        shouldBe(receiver[key], TEST_VALUE, key);
    } else
        shouldThrowTypeError(() => { receiver[key] = TEST_VALUE; }, key);
}

function shouldBe(actual, expected, key) {
    if (actual !== expected)
        throw new Error(`Key: ${String(key)}. ${actual} !== ${expected}`);
}

function shouldThrowTypeError(func, key) {
    let errorThrown = false;
    try {
        func();
    } catch (error) {
        errorThrown = true;
        if (error.constructor !== TypeError)
            throw new Error(`Key: ${String(key)}. Bad error: ${error}`);
    }
    if (!errorThrown)
        throw new Error(`Key: ${String(key)}. Didn't throw!`);
}
