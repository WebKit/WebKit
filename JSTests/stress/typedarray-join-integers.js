function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function shouldThrowTypeError(func) {
    let error;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof TypeError))
        throw new Error('Expected TypeError');
}

const separators = [undefined, '', ',', '--', '\u3042', 'ab\u3042cd'];

function reference(array, separator) {
    return Array.prototype.map.call(array, String).join(separator);
}

function check(array) {
    for (const separator of separators)
        shouldBe(array.join(separator), reference(array, separator));
}

const numberTypes = [
    [Int8Array, -128, 127],
    [Uint8Array, 0, 255],
    [Uint8ClampedArray, 0, 255],
    [Int16Array, -32768, 32767],
    [Uint16Array, 0, 65535],
    [Int32Array, -2147483648, 2147483647],
    [Uint32Array, 0, 4294967295],
];

const bigIntTypes = [
    [BigInt64Array, -(2n ** 63n), 2n ** 63n - 1n],
    [BigUint64Array, 0n, 2n ** 64n - 1n],
];

for (const [constructor, min, max] of numberTypes) {
    check(new constructor(0));
    check(new constructor([0]));
    check(new constructor([max]));
    check(new constructor([min, max, 0, 1, 9, 10, 99, 100, 101, max - 1, min + 1]));
    const big = new constructor(1000);
    for (let i = 0; i < big.length; ++i)
        big[i] = (i * 2654435761) % (max - min + 1) + min;
    check(big);
}

for (const [constructor, min, max] of bigIntTypes) {
    check(new constructor(0));
    check(new constructor([0n]));
    check(new constructor([max]));
    check(new constructor([min, max, 0n, 1n, 9n, 10n, 99n, 100n, max - 1n, min + 1n]));
    const big = new constructor(1000);
    for (let i = 0; i < big.length; ++i)
        big[i] = BigInt(i) * 0x9e3779b97f4a7c15n;
    check(big);
}

for (const [constructor] of [...numberTypes, ...bigIntTypes]) {
    const one = constructor === BigInt64Array || constructor === BigUint64Array ? 1n : 1;

    {
        const array = new constructor(4).fill(one);
        shouldBe(array.join({ toString() { transferArrayBuffer(array.buffer); return '-'; } }), '---');
        shouldThrowTypeError(() => array.join());
    }

    {
        const buffer = new ArrayBuffer(4 * constructor.BYTES_PER_ELEMENT, { maxByteLength: 8 * constructor.BYTES_PER_ELEMENT });
        const array = new constructor(buffer, 0, 4).fill(one);
        shouldBe(array.join({ toString() { buffer.resize(2 * constructor.BYTES_PER_ELEMENT); return ','; } }), ',,,');
        shouldThrowTypeError(() => array.join());
    }

    {
        const buffer = new ArrayBuffer(4 * constructor.BYTES_PER_ELEMENT, { maxByteLength: 8 * constructor.BYTES_PER_ELEMENT });
        const array = new constructor(buffer).fill(one);
        shouldBe(array.join({ toString() { buffer.resize(2 * constructor.BYTES_PER_ELEMENT); return '\u3042'; } }), '1\u30421\u3042\u3042');
        shouldBe(array.join(), '1,1');
        buffer.resize(8 * constructor.BYTES_PER_ELEMENT);
        shouldBe(array.join(''), '11000000');
    }

    {
        const buffer = new ArrayBuffer(3 * constructor.BYTES_PER_ELEMENT, { maxByteLength: 3 * constructor.BYTES_PER_ELEMENT });
        const array = new constructor(buffer).fill(one);
        shouldBe(array.join({ toString() { buffer.resize(0); return ''; } }), '');
    }

    if (typeof SharedArrayBuffer !== 'undefined') {
        const array = new constructor(new SharedArrayBuffer(3 * constructor.BYTES_PER_ELEMENT)).fill(one);
        shouldBe(array.join(), '1,1,1');
        check(array);
    }
}

for (const [constructor, min, max] of numberTypes) {
    const boundaries = [];
    for (let power = 1; power <= max; power *= 10) {
        for (const value of [power - 1, power, -(power - 1), -power]) {
            if (value >= min && value <= max)
                boundaries.push(value);
        }
    }
    check(new constructor(boundaries));
}

for (const [constructor, min, max] of bigIntTypes) {
    const boundaries = [];
    for (let power = 1n; power <= max; power *= 10n) {
        for (const value of [power - 1n, power, -(power - 1n), -power]) {
            if (value >= min && value <= max)
                boundaries.push(value);
        }
    }
    check(new constructor(boundaries));
}
