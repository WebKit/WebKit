// The original is https://github.com/tc39/test262/blob/b1f9a0aea3e5d12563680ba3c8eee275774f9316/test/built-ins/TypedArrayConstructors/ctors/typedarray-arg/throw-type-error-before-custom-proto-access.js

function shouldThrow(caseName, fn, expectedErrorCtor, expectedErrorMessage) {
    if (!caseName)
        throw new Error(`must specify test case name`);

    const expected = `${expectedErrorCtor.name}(${expectedErrorMessage})`;
    try {
        fn();
        throw new Error(`${caseName}: Expected to throw ${expected}, but succeeded`);
    } catch (e) {
        const actual = `${e.name}(${e.message})`;
        if (!(e instanceof expectedErrorCtor) || e.message !== expectedErrorMessage)
            throw new Error(`${caseName}: Expected ${expected} but got ${actual}`);
    }
}

const newTarget = function () { }.bind(null);
Object.defineProperty(newTarget, "prototype", {
    get() {
        throw new Error('this should not be caught');
    },
});

const TEST_TARGET = [
    BigInt64Array,
    BigUint64Array,
    Float16Array,
    Float32Array,
    Float64Array,
    Int16Array,
    Int32Array,
    Int8Array,
    Uint16Array,
    Uint32Array,
    Uint8Array,
    Uint8ClampedArray,
];

for (const Ctor of TEST_TARGET) {
    const label = Ctor.name;

    // https://tc39.es/ecma262/2026/#sec-typedarray
    // If the 1st argument is not an Object,
    // the step 9 |ToIndex(firstArgument)| should be executed before the step 10 |AllocateTypedArray|.
    shouldThrow(label, () => {
        Reflect.construct(Ctor, [Symbol()], newTarget);
    }, TypeError, 'Cannot convert a symbol to a number');
}