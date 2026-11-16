function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

const constructors = [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, Float16Array, Float32Array, Float64Array];
const bigIntConstructors = [BigInt64Array, BigUint64Array];

function shrinkWhileConverting(constructor) {
    let values = [0, { valueOf() { values.length = 0; return 100; } }, 2];
    let array = new constructor(values);
    shouldBe(array.length, 3);
    shouldBe(array[0], 0);
    shouldBe(array[1], 100);
    shouldBe(array[2], 2);
}

function overwriteWhileConverting(constructor) {
    let values = [0, { valueOf() { values[2] = 42; return 100; } }, 2];
    let array = new constructor(values);
    shouldBe(array.length, 3);
    shouldBe(array[0], 0);
    shouldBe(array[1], 100);
    shouldBe(array[2], 2);
}

function growWhileConverting(constructor) {
    let values = [0, { valueOf() { values.push(42); return 100; } }, 2];
    let array = new constructor(values);
    shouldBe(array.length, 3);
    shouldBe(array[2], 2);
}

function shrinkWhileConvertingBigInt(constructor) {
    let values = [0n, { valueOf() { values.length = 0; return 100n; } }, 2n];
    let array = new constructor(values);
    shouldBe(array.length, 3);
    shouldBe(array[0], 0n);
    shouldBe(array[1], 100n);
    shouldBe(array[2], 2n);
}

for (let i = 0; i < testLoopCount; ++i) {
    for (const constructor of constructors) {
        shrinkWhileConverting(constructor);
        overwriteWhileConverting(constructor);
        growWhileConverting(constructor);
    }
    for (const constructor of bigIntConstructors)
        shrinkWhileConvertingBigInt(constructor);

    shouldBe(new Int32Array([1, 2, 3]).join(), "1,2,3");
    shouldBe(new Float64Array([1.5, 2.5]).join(), "1.5,2.5");
    shouldBe(new Int32Array([1, , 3]).join(), "1,0,3");
    shouldBe(new Int32Array(["1", "2"]).join(), "1,2");
}

// A sparse index forces array storage, which reaches the same conversion loop.
{
    let values = [0, { valueOf() { values.length = 0; return 100; } }, 2];
    values[100000] = 3;
    let array = new Uint8Array(values);
    shouldBe(array.length, 100001);
    shouldBe(array[0], 0);
    shouldBe(array[1], 100);
    shouldBe(array[2], 2);
    shouldBe(array[100000], 3);
}
