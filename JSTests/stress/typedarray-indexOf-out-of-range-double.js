// indexOf, includes, and lastIndexOf must not match a double that is outside the range of the element type.
// The search values are read from a Float64Array, so they are double JSValues.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

const types = [
    [Int8Array, Int8Array, -128, 127],
    [Uint8Array, Uint8Array, 0, 255],
    [Uint8ClampedArray, Uint8Array, 0, 255],
    [Int16Array, Int16Array, -32768, 32767],
    [Uint16Array, Uint16Array, 0, 65535],
    [Int32Array, Int32Array, -2147483648, 2147483647],
    [Uint32Array, Uint32Array, 0, 4294967295],
];

const candidates = [256, 257, 300, -1, -129, 32768, 65536, 65537, -32769, 2 ** 31, -(2 ** 31) - 1, 2 ** 32, 2 ** 32 + 1, -(2 ** 32), 1e10, 2 ** 53, 2 ** 63 - 1024, 2 ** 63, 2 ** 63 + 2048, -(2 ** 63) + 1024, -(2 ** 63), -(2 ** 63) - 2048, 1e300, -1e300, Infinity, -Infinity];

for (const [constructor, wrappingConstructor, min, max] of types) {
    const outOfRange = new Float64Array(candidates.filter(value => value < min || value > max));
    for (let i = 0; i < outOfRange.length; ++i) {
        const value = outOfRange[i];
        // Put the value that a wrapping conversion gives, and the ends of the range, in the array.
        const wrapped = new wrappingConstructor([value])[0];
        const array = new constructor([0, 1, 44, min, max, wrapped]);
        const message = `${constructor.name}, value ${value}`;
        shouldBe(array.indexOf(value), -1, `${message}, indexOf`);
        shouldBe(array.includes(value), false, `${message}, includes`);
        shouldBe(array.lastIndexOf(value), -1, `${message}, lastIndexOf`);
    }

    // Doubles in the range still match.
    const array = new constructor([0, 1, min, max]);
    const inRange = new Float64Array([1, min, max, -0]);
    shouldBe(array.indexOf(inRange[0]), 1, `${constructor.name}, indexOf 1`);
    shouldBe(array.indexOf(inRange[1]), min === 0 ? 0 : 2, `${constructor.name}, indexOf min`);
    shouldBe(array.lastIndexOf(inRange[2]), 3, `${constructor.name}, lastIndexOf max`);
    shouldBe(array.includes(inRange[3]), true, `${constructor.name}, includes -0`);

    // Fractions, including the ones next to the ends of the range, and NaN do not match.
    const noMatch = new Float64Array([1.5, min - 0.5, min + 0.5, max - 0.5, max + 0.5, NaN]);
    for (let i = 0; i < noMatch.length; ++i) {
        const value = noMatch[i];
        const message = `${constructor.name}, value ${value}`;
        shouldBe(array.indexOf(value), -1, `${message}, indexOf`);
        shouldBe(array.includes(value), false, `${message}, includes`);
        shouldBe(array.lastIndexOf(value), -1, `${message}, lastIndexOf`);
    }
}

// The case from the bug report.
const array = new Uint8ClampedArray([1]);
shouldBe(array.indexOf(257.0), -1, "Uint8ClampedArray, indexOf 257.0");
shouldBe(array.includes(257.0), false, "Uint8ClampedArray, includes 257.0");
shouldBe(array.lastIndexOf(257.0), -1, "Uint8ClampedArray, lastIndexOf 257.0");
