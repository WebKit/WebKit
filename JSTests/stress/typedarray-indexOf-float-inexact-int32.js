// indexOf, includes, and lastIndexOf must not match an int32 that the float element type cannot represent exactly.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

function test(array, value, expectedIndex) {
    const message = `${array.constructor.name}, value ${value}`;
    shouldBe(array.indexOf(value), expectedIndex, `${message}, indexOf`);
    shouldBe(array.includes(value), expectedIndex !== -1, `${message}, includes`);
    shouldBe(array.lastIndexOf(value), expectedIndex, `${message}, lastIndexOf`);
}

const int32Max = 2 ** 31 - 1;
const int32Min = -(2 ** 31);

// [constructor, int32 values that are exact, int32 values that are not exact]
const types = [
    [Float16Array,
        [0, 1, 2047, 2048, 2050, 4096, 4100, 65472, 65504],
        [2049, 2051, 4097, 4098, 65503, 65505, 65519, 65520, 65535, 65536, 70000, 16777216, int32Max]],
    [Float32Array,
        [0, 1, 2049, 65520, 70000, 16777215, 16777216, 16777218, 33554436, 2 ** 30, int32Max - 127],
        [16777217, 16777219, 33554433, 33554434, 2 ** 30 + 1, int32Max - 128, int32Max - 1, int32Max]],
    [Float64Array,
        [0, 1, 2049, 70000, 16777217, int32Max],
        []],
];

for (const [constructor, exact, inexact] of types) {
    for (const value of exact) {
        test(new constructor([0.5, value]), value, 1);
        test(new constructor([0.5, -value]), -value, 1);
    }

    for (const value of inexact) {
        // The second element is the value that a rounding conversion gives.
        test(new constructor([0.5, value, Infinity, 0.25]), value, -1);
        test(new constructor([0.5, -value, -Infinity, 0.25]), -value, -1);

        // A value with a double representation gives the same result.
        test(new constructor([0.5, value, Infinity, 0.25]), new Float64Array([value])[0], -1);
        test(new constructor([0.5, -value, -Infinity, 0.25]), new Float64Array([-value])[0], -1);
    }

    // A power of two is always exact here, if it is in the range.
    test(new constructor([0.5, int32Min]), int32Min, constructor === Float16Array ? -1 : 1);

    // Infinity itself still matches.
    test(new constructor([0.5, Infinity]), Infinity, 1);
    test(new constructor([0.5, -Infinity]), -Infinity, 1);
}
