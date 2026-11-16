function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value: expected ${String(expected)} but got ${String(actual)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(Array.isArray(actual), true);
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function shouldThrow(func, errorType) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof errorType))
        throw new Error(`bad error: ${String(error)}`);
}

function spread(iterable) {
    return [...iterable];
}
noInline(spread);

function spreadWithOthers(iterable) {
    return [1, ...iterable, 2, ...iterable];
}
noInline(spreadWithOthers);

function collect() {
    return Array.prototype.slice.call(arguments);
}
noInline(collect);

function callSpread(iterable) {
    return collect(...iterable);
}
noInline(callSpread);

class Collector {
    constructor(...args) {
        this.args = args;
    }
}

function constructSpread(iterable) {
    return new Collector(...iterable).args;
}
noInline(constructSpread);

function elements(typedArray) {
    let result = [];
    for (let i = 0; i < typedArray.length; ++i)
        result.push(typedArray[i]);
    return result;
}

function makeTypedArray(constructor, values) {
    let typedArray = new constructor(19);
    for (let i = 0; i < typedArray.length; ++i)
        typedArray[i] = values[i % values.length];
    return typedArray;
}

const typedArrays = [
    makeTypedArray(Int8Array, [-128, -1, 0, 1, 127]),
    makeTypedArray(Uint8Array, [0, 1, 128, 255]),
    makeTypedArray(Uint8ClampedArray, [0, 1, 128, 255]),
    makeTypedArray(Int16Array, [-32768, -1, 0, 32767]),
    makeTypedArray(Uint16Array, [0, 1, 32768, 65535]),
    makeTypedArray(Int32Array, [-2147483648, -1, 0, 2147483647]),
    makeTypedArray(Uint32Array, [0, 1, 2147483647, 2147483648, 4294967295]),
    makeTypedArray(Float16Array, [0, -0, 1.5, NaN, Infinity, -Infinity, 65504]),
    makeTypedArray(Float32Array, [0, -0, 1.5, NaN, Infinity, -Infinity, 16777216, 0.1]),
    makeTypedArray(Float64Array, [0, -0, 1.5, NaN, Infinity, -Infinity, 9007199254740992, 0.1, 4294967296]),
    makeTypedArray(BigInt64Array, [-9223372036854775808n, -1n, 0n, 1n, 9223372036854775807n]),
    makeTypedArray(BigUint64Array, [0n, 1n, 9223372036854775808n, 18446744073709551615n]),
    new Uint8Array(0),
    new Uint16Array(new Uint16Array([1, 2, 3, 4, 5, 6]).buffer, 4, 3),
];
const expectedElements = typedArrays.map(elements);
const expectedElementsWithOthers = expectedElements.map(expected => [1, ...expected, 2, ...expected]);

for (let i = 0; i < testLoopCount; ++i) {
    let index = i % typedArrays.length;
    let typedArray = typedArrays[index];
    let expected = expectedElements[index];
    shouldBeArray(spread(typedArray), expected);
    shouldBeArray(callSpread(typedArray), expected);
    shouldBeArray(constructSpread(typedArray), expected);
    shouldBeArray(spreadWithOthers(typedArray), expectedElementsWithOthers[index]);
}

{
    let typedArray = new Uint8Array([1, 2, 3]);
    let first = spread(typedArray);
    let second = spread(typedArray);
    shouldBe(first === second, false);
    first[0] = 42;
    first.push(4);
    shouldBeArray(first, [42, 2, 3, 4]);
    shouldBeArray(second, [1, 2, 3]);
    shouldBeArray(elements(typedArray), [1, 2, 3]);
    typedArray[1] = 9;
    shouldBeArray(second, [1, 2, 3]);
    shouldBeArray(spread(typedArray), [1, 9, 3]);
}

{
    let typedArray = new Uint8Array([1, 2, 3]);
    typedArray.buffer.transfer();
    shouldThrow(() => spread(typedArray), TypeError);
}

shouldThrow(() => spread(new DataView(new ArrayBuffer(8))), TypeError);

{
    let buffer = new ArrayBuffer(8, { maxByteLength: 16 });
    let whole = new Uint8Array(buffer);
    for (let i = 0; i < 8; ++i)
        whole[i] = i + 1;
    let fixed = new Uint8Array(buffer, 4, 4);
    let tracking = new Uint8Array(buffer, 4);
    let trackingUint16 = new Uint16Array(buffer, 2);
    shouldBeArray(spread(whole), [1, 2, 3, 4, 5, 6, 7, 8]);
    shouldBeArray(spread(fixed), [5, 6, 7, 8]);
    shouldBeArray(spread(tracking), [5, 6, 7, 8]);
    shouldBeArray(spread(trackingUint16), elements(trackingUint16));

    buffer.resize(7);
    shouldBeArray(spread(whole), [1, 2, 3, 4, 5, 6, 7]);
    shouldThrow(() => spread(fixed), TypeError);
    shouldBeArray(spread(tracking), [5, 6, 7]);
    shouldBeArray(spread(trackingUint16), elements(trackingUint16));

    buffer.resize(3);
    shouldBeArray(spread(whole), [1, 2, 3]);
    shouldThrow(() => spread(fixed), TypeError);
    shouldThrow(() => spread(tracking), TypeError);
    shouldBeArray(spread(trackingUint16), []);

    buffer.resize(1);
    shouldThrow(() => spread(trackingUint16), TypeError);

    buffer.resize(16);
    shouldBeArray(spread(whole), [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    shouldBeArray(spread(fixed), [0, 0, 0, 0]);
    shouldBeArray(spread(tracking), [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
    shouldBeArray(spread(trackingUint16), [0, 0, 0, 0, 0, 0, 0]);
}

{
    let buffer = new SharedArrayBuffer(4, { maxByteLength: 8 });
    let tracking = new Uint8Array(buffer);
    let fixed = new Uint8Array(buffer, 1, 2);
    tracking.set([1, 2, 3, 4]);
    shouldBeArray(spread(tracking), [1, 2, 3, 4]);
    shouldBeArray(spread(fixed), [2, 3]);
    buffer.grow(8);
    shouldBeArray(spread(tracking), [1, 2, 3, 4, 0, 0, 0, 0]);
    shouldBeArray(spread(fixed), [2, 3]);
}

{
    let typedArray = new Uint8Array([1, 2, 3]);
    typedArray[Symbol.iterator] = function* () {
        yield 42;
    };
    shouldBeArray(spread(typedArray), [42]);
}

{
    class Derived extends Uint8Array { }
    shouldBeArray(spread(new Derived([1, 2, 3])), [1, 2, 3]);

    class DerivedWithIterator extends Uint8Array {
        *[Symbol.iterator]() {
            yield 42;
        }
    }
    shouldBeArray(spread(new DerivedWithIterator([1, 2, 3])), [42]);
}

{
    let typedArray = new Uint8Array([1, 2, 3]);
    Object.setPrototypeOf(typedArray, {
        __proto__: Uint8Array.prototype,
        *[Symbol.iterator]() {
            yield 42;
        }
    });
    shouldBeArray(spread(typedArray), [42]);
}

function testInRealm(modify, check) {
    let realm = createGlobalObject();
    let typedArrays = [
        new realm.Uint8Array([1, 2, 3]),
        new realm.Float64Array([1.5, 2.5]),
        new realm.BigInt64Array([1n, 2n]),
    ];
    shouldBeArray(spread(typedArrays[0]), [1, 2, 3]);
    shouldBeArray(spread(typedArrays[1]), [1.5, 2.5]);
    shouldBeArray(spread(typedArrays[2]), [1n, 2n]);
    modify(realm);
    check(typedArrays);
    shouldBeArray(spread(new Uint8Array([1, 2, 3])), [1, 2, 3]);
    shouldBeArray(spread(new Float64Array([1.5, 2.5])), [1.5, 2.5]);
}

testInRealm(realm => {
    realm.Uint8Array.prototype[Symbol.iterator] = function* () {
        yield 42;
    };
}, typedArrays => {
    shouldBeArray(spread(typedArrays[0]), [42]);
    shouldBeArray(spread(typedArrays[1]), [1.5, 2.5]);
    shouldBeArray(spread(typedArrays[2]), [1n, 2n]);
});

testInRealm(realm => {
    Object.getPrototypeOf(realm.Uint8Array.prototype)[Symbol.iterator] = function* () {
        yield this.length;
    };
}, typedArrays => {
    shouldBeArray(spread(typedArrays[0]), [3]);
    shouldBeArray(spread(typedArrays[1]), [2]);
    shouldBeArray(spread(typedArrays[2]), [2]);
});

testInRealm(realm => {
    let arrayIteratorPrototype = Object.getPrototypeOf(new realm.Array()[Symbol.iterator]());
    let count = 0;
    arrayIteratorPrototype.next = function () {
        return { done: count++ & 1, value: 42 };
    };
}, typedArrays => {
    shouldBeArray(spread(typedArrays[0]), [42]);
    shouldBeArray(spread(typedArrays[1]), [42]);
    shouldBeArray(spread(typedArrays[2]), [42]);
});

testInRealm(realm => {
    Object.setPrototypeOf(realm.Float64Array.prototype, {
        __proto__: Object.getPrototypeOf(realm.Float64Array.prototype),
        *[Symbol.iterator]() {
            yield 42;
        }
    });
}, typedArrays => {
    shouldBeArray(spread(typedArrays[0]), [1, 2, 3]);
    shouldBeArray(spread(typedArrays[1]), [42]);
    shouldBeArray(spread(typedArrays[2]), [1n, 2n]);
});
