function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

const separators = [undefined, '', ',', '--', '\u3042', 'ab\u3042cd'];

function reference(array, separator) {
    return Array.prototype.map.call(array, String).join(separator);
}

function check(array) {
    for (const separator of separators)
        shouldBe(array.join(separator), reference(array, separator));
}

let seed = 42;
function next() {
    seed = (seed + 0x9e3779b9) | 0;
    let t = seed ^ (seed >>> 16);
    t = Math.imul(t, 0x21f0aaad);
    t = t ^ (t >>> 15);
    t = Math.imul(t, 0x735a2d97);
    return (t ^ (t >>> 15)) >>> 0;
}

const values = [
    0, -0, 1, -1, 0.5, 0.1, -0.1, 1 / 3, 1e21, 1e-7, 123456789.125, 1e20, 123e-20,
    2 ** 53 - 1, 2 ** 53, 2 ** 53 + 2, -(2 ** 53), 2 ** 60, 2 ** 64,
    Number.MAX_VALUE, Number.MIN_VALUE, NaN, Infinity, -Infinity, 65504, 6.103515625e-5,
];

function fillRandomBits(array) {
    const words = new Uint32Array(array.buffer, array.byteOffset, array.byteLength / 4);
    for (let i = 0; i < words.length; ++i)
        words[i] = next();
}

function fillRandomNumbers(array) {
    for (let i = 0; i < array.length; ++i)
        array[i] = (next() / 0x100000000 - 0.5) * 10 ** ((next() % 40) - 20);
}

for (const constructor of [Float16Array, Float32Array, Float64Array]) {
    check(new constructor(0));
    check(new constructor([0.5]));
    check(new constructor(values));
    for (const value of values)
        check(new constructor([value]));

    const bits = new constructor(1024);
    fillRandomBits(bits);
    check(bits);

    const numbers = new constructor(1024);
    fillRandomNumbers(numbers);
    check(numbers);

    const shared = new constructor(new SharedArrayBuffer(1024 * constructor.BYTES_PER_ELEMENT));
    fillRandomNumbers(shared);
    check(shared);

    {
        const array = new constructor(4).fill(0.5);
        shouldBe(array.join({ toString() { transferArrayBuffer(array.buffer); return '-'; } }), '---');
    }

    {
        const buffer = new ArrayBuffer(4 * constructor.BYTES_PER_ELEMENT, { maxByteLength: 8 * constructor.BYTES_PER_ELEMENT });
        const array = new constructor(buffer).fill(0.5);
        shouldBe(array.join({ toString() { buffer.resize(2 * constructor.BYTES_PER_ELEMENT); return '\u3042'; } }), '0.5\u30420.5\u3042\u3042');
    }

    {
        const buffer = new SharedArrayBuffer(2 * constructor.BYTES_PER_ELEMENT, { maxByteLength: 8 * constructor.BYTES_PER_ELEMENT });
        const array = new constructor(buffer).fill(0.5);
        shouldBe(array.join({ toString() { buffer.grow(4 * constructor.BYTES_PER_ELEMENT); return ','; } }), '0.5,0.5');
        shouldBe(array.join(), '0.5,0.5,0,0');
    }
}

for (const constructor of [Int8Array, Uint8Array, Uint8ClampedArray, Int16Array, Uint16Array, Int32Array, Uint32Array, BigInt64Array, BigUint64Array]) {
    const shared = new constructor(new SharedArrayBuffer(1024 * constructor.BYTES_PER_ELEMENT));
    fillRandomBits(shared);
    check(shared);
    check(new constructor(new SharedArrayBuffer(0)));
}
