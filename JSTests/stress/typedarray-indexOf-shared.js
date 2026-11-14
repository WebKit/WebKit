//@ runDefault

// With some C libraries, indexOf, includes, and lastIndexOf on Int8Array, Uint8Array, and Uint8ClampedArray search shared memory with a separate loop.
// Check that these functions give the same results as a reference implementation of the spec.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${String(expected)} but got ${String(actual)}`);
}

function toIntegerOrInfinity(value) {
    const number = Number(value);
    if (Number.isNaN(number))
        return 0;
    return Math.trunc(number) + 0;
}

function referenceIndexOf(array, value, fromIndex) {
    const length = array.length;
    let k = toIntegerOrInfinity(fromIndex);
    if (k === Infinity)
        return -1;
    if (k < 0)
        k = Math.max(length + k, 0);
    for (; k < length; ++k) {
        if (array[k] === value)
            return k;
    }
    return -1;
}

function referenceLastIndexOf(array, value, fromIndex, hasFromIndex) {
    const length = array.length;
    let k = hasFromIndex ? toIntegerOrInfinity(fromIndex) : length - 1;
    if (k === -Infinity)
        return -1;
    k = k >= 0 ? Math.min(k, length - 1) : length + k;
    for (; k >= 0; --k) {
        if (array[k] === value)
            return k;
    }
    return -1;
}

function referenceIncludes(array, value, fromIndex) {
    const length = array.length;
    let k = toIntegerOrInfinity(fromIndex);
    if (k === Infinity)
        return false;
    if (k < 0)
        k = Math.max(length + k, 0);
    for (; k < length; ++k) {
        const element = array[k];
        if (element === value || (element !== element && value !== value))
            return true;
    }
    return false;
}

const fromIndices = [0, 1, 16, 64, -1, -17, -65, 1e9, Infinity, -Infinity, NaN];

function check(array, value, label) {
    const copy = new array.constructor(array);
    const message = `${label}, value ${String(value)}`;

    shouldBe(array.indexOf(value), referenceIndexOf(array, value, 0), `${message}, indexOf`);
    shouldBe(array.includes(value), referenceIncludes(array, value, 0), `${message}, includes`);
    shouldBe(array.lastIndexOf(value), referenceLastIndexOf(array, value, undefined, false), `${message}, lastIndexOf`);
    shouldBe(array.indexOf(value), copy.indexOf(value), `${message}, indexOf (copy)`);
    shouldBe(array.includes(value), copy.includes(value), `${message}, includes (copy)`);
    shouldBe(array.lastIndexOf(value), copy.lastIndexOf(value), `${message}, lastIndexOf (copy)`);

    for (const fromIndex of fromIndices) {
        const indexMessage = `${message}, fromIndex ${fromIndex}`;
        shouldBe(array.indexOf(value, fromIndex), referenceIndexOf(array, value, fromIndex), `${indexMessage}, indexOf`);
        shouldBe(array.includes(value, fromIndex), referenceIncludes(array, value, fromIndex), `${indexMessage}, includes`);
        shouldBe(array.lastIndexOf(value, fromIndex), referenceLastIndexOf(array, value, fromIndex, true), `${indexMessage}, lastIndexOf`);
    }
}

const lengths = [0, 1, 2, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 300];
const byteOffsets = [0, 3];

for (const constructor of [Int8Array, Uint8Array, Uint8ClampedArray]) {
    const targets = constructor === Int8Array ? [1, -1] : [1, 255];
    const absent = constructor === Int8Array ? [7, 255, 1.5, "1"] : [7, -1, 1.5, "1"];

    for (const length of lengths) {
        for (const byteOffset of byteOffsets) {
            const array = new constructor(new SharedArrayBuffer(byteOffset + length), byteOffset, length);
            const label = `${constructor.name}, length ${length}, byteOffset ${byteOffset}`;

            for (const value of [0, ...absent])
                check(array, value, `${label}, all zero`);

            const positions = [...new Set([0, 1, 15, 16, length >> 1, length - 17, length - 16, length - 1])]
                .filter(position => position >= 0 && position < length)
                .sort((a, b) => a - b);
            for (const target of targets) {
                for (const position of positions) {
                    array[position] = target;
                    check(array, target, `${label}, one target at ${position}`);
                    array[position] = 0;
                }

                if (positions.length >= 3) {
                    const first = positions[1];
                    const last = positions[positions.length - 1];
                    array[first] = target;
                    array[last] = target;
                    check(array, target, `${label}, targets at ${first} and ${last}`);
                    shouldBe(array.indexOf(target, first + 1), last, `${label}, indexOf after first`);
                    shouldBe(array.lastIndexOf(target, last - 1), first, `${label}, lastIndexOf before last`);
                    array[first] = 0;
                    array[last] = 0;
                }
            }

            for (const value of absent) {
                array.fill(0xff);
                check(array, value, `${label}, all 0xff`);
                array.fill(0);
            }
        }
    }

    // A length-tracking view on a growable SharedArrayBuffer.
    const growable = new SharedArrayBuffer(16, { maxByteLength: 4096 });
    const tracking = new constructor(growable);
    shouldBe(tracking.indexOf(1), -1, `${constructor.name}, growable, before grow`);
    growable.grow(1000);
    shouldBe(tracking.length, 1000, `${constructor.name}, growable, length after grow`);
    tracking[900] = 1;
    tracking[100] = 1;
    check(tracking, 1, `${constructor.name}, growable, after grow`);
}
