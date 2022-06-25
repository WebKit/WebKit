const TEST_VALUE = 1062630713; // Array's "length" is required to be int32
const cleanUpMap = new WeakMap;
const hasOwn = (obj, key) => Object.prototype.hasOwnProperty.call(obj, key);

testSetResult(() => [1, 2], "length", true);
testSetResult(
    () => Object.defineProperty(new Array, "length", { writable: false }),
    "length",
    false,
);

testSetResult(() => new String, "length", false);
testSetResult(() => new RegExp, "lastIndex", true);
testSetResult(
    () => Object.defineProperty(/(?:)/g, "lastIndex", { writable: false }),
    "lastIndex",
    false,
);

for (const key of ["line", "column", "sourceURL", "stack"])
    testSetResult(() => new Error, key, true);

const mappedArguments = () => (function(a, b) { return arguments; })(1, 2);
testSetResult(mappedArguments, "callee", true);
testSetResult(mappedArguments, "length", true);
testSetResult(mappedArguments, Symbol.iterator, true);

const unmappedArguments = function() { "use strict"; return arguments; };
testSetResult(unmappedArguments, "length", true);
testSetResult(unmappedArguments, Symbol.iterator, true);

var foo;
function bar() {}
for (const key of ["foo", "bar", "parseInt"])
    testSetResult(() => globalThis, key, true);

for (const key of ["Infinity", "NaN", "undefined"])
    testSetResult(() => $vm.createGlobalObject().globalThis, key, false);

for (const key of ["name", "length"]) {
    testSetResult(() => function() {}.bind(), key, false);
    testSetResult(
        () => Object.defineProperty(function() {}, key, { writable: true }),
        key,
        true,
    );

    testSetResult(() => () => { "use strict"; }, key, false);
    testSetResult(
        () => Object.defineProperty(parseFloat, key, { writable: true }),
        key,
        true,
    );
}

testSetResult(() => function() { "use strict"; }, "prototype", true);
testSetResult(
    () => Object.defineProperty(function() {}, "prototype", { writable: false }),
    "prototype",
    false,
);

// === harness ===

function testSetResult(getTarget, key, expectedResult) {
    for (const primitive of [true, 1, "foo", Symbol(), 0n]) {
        const primitivePrototype = Object.getPrototypeOf(primitive);
        const getTargetWithDummySetterOnPrototype = withSetterOnPrototype(() => {
            const target = getTarget();
            if (Object.getPrototypeOf(target) !== primitivePrototype)
                Object.setPrototypeOf(primitivePrototype, target);
            return target;
        }, key, () => {});

        testSetResultInternal(getTargetWithDummySetterOnPrototype, () => primitive, key, false);
        Object.setPrototypeOf(primitivePrototype, Object.prototype);
    }

    const getTargetWithPoisonedSetterOnPrototype = withSetterOnPrototype(getTarget, key, () => { throw new Error(`${String(key)} setter should be unreachable!`); });
    testSetResultInternal(getTargetWithPoisonedSetterOnPrototype, t => Object.create(t), key, expectedResult);
    testSetResultInternal(getTargetWithPoisonedSetterOnPrototype, t => Object.create(Object.create(t)), key, expectedResult);
    testSetResultInternal(getTargetWithPoisonedSetterOnPrototype, t => new Proxy(t, {}), key, expectedResult);
    testSetResultInternal(getTargetWithPoisonedSetterOnPrototype, t => new Proxy(t, Reflect), key, expectedResult);
    testSetResultInternal(getTargetWithPoisonedSetterOnPrototype, t => $vm.createProxy(Object.create(t)), key, expectedResult);
}

function withSetterOnPrototype(getTarget, key, set) {
    "use strict";
    return () => {
        const target = getTarget();
        const targetPrototype = Object.getPrototypeOf(target);
        const newDescriptor = { set, configurable: true };

        if (key === "length" && !Object.getOwnPropertyDescriptor(target, key).configurable) {
            const newPrototype = Object.create(null, { [key]: newDescriptor });
            Object.setPrototypeOf(target, newPrototype);
            cleanUpMap.set(target, () => { Object.setPrototypeOf(target, targetPrototype); });
        } else {
            const prevDescriptor = Object.getOwnPropertyDescriptor(targetPrototype, key);
            Object.defineProperty(targetPrototype, key, newDescriptor);
            cleanUpMap.set(target, prevDescriptor
                ? () => { Object.defineProperty(targetPrototype, key, prevDescriptor); }
                : () => { delete targetPrototype[key]; });
        }

        return target;
    };
}

function testSetResultInternal(getTarget, getReceiver, key, expectedResult) {
    testSetResultSloppy(getTarget, getReceiver, key, expectedResult);
    testSetResultStrict(getTarget, getReceiver, key, expectedResult);
    testSetResultReflectSet(getTarget, getReceiver, key, expectedResult);
}

function testSetResultSloppy(getTarget, getReceiver, key, expectedResult) {
    const target = getTarget();
    const receiver = getReceiver(target);

    receiver[key] = TEST_VALUE;
    shouldBe(receiver[key] === TEST_VALUE, expectedResult, key);
    if (expectedResult)
        shouldBe(hasOwn(receiver, key), true, key);
    cleanUpMap.get(target)();
}

function testSetResultStrict(getTarget, getReceiver, key, expectedResult) {
    "use strict";
    const target = getTarget();
    const receiver = getReceiver(target);

    if (expectedResult) {
        receiver[key] = TEST_VALUE;
        shouldBe(receiver[key], TEST_VALUE, key);
        shouldBe(hasOwn(receiver, key), true, key);
    } else
        shouldThrowTypeError(() => { receiver[key] = TEST_VALUE; }, key);
    cleanUpMap.get(target)();
}

function testSetResultReflectSet(getTarget, getReceiver, key, expectedResult) {
    "use strict";
    const target = getTarget();
    const receiver = getReceiver(target);

    if (receiver === Object(receiver))
        shouldBe(Reflect.set(receiver, key, TEST_VALUE), expectedResult, key);
    shouldBe(Reflect.set(target, key, TEST_VALUE, receiver), expectedResult, key);
    if (expectedResult)
        shouldBe(hasOwn(receiver, key), true, key);
    cleanUpMap.get(target)();
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
