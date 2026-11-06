function shouldBe(actual, expected, operation, value, littleEndian) {
    if (!Object.is(actual, expected))
        throw new Error(`bad value for ${operation} of ${value} (littleEndian: ${littleEndian}): ${actual} expected ${expected}`);
}

function halfBitsToDouble(bits) {
    let sign = (bits & 0x8000) ? -1 : 1;
    let exponent = (bits >> 10) & 0x1f;
    let mantissa = bits & 0x3ff;
    if (exponent === 0x1f)
        return mantissa ? NaN : sign * Infinity;
    if (exponent === 0)
        return sign * mantissa * 2 ** -24;
    return sign * (1024 + mantissa) * 2 ** (exponent - 25);
}

function referenceRound(value) {
    if (Number.isNaN(value) || value === 0 || !Number.isFinite(value))
        return value;
    let sign = value < 0 ? -1 : 1;
    let magnitude = Math.abs(value);
    if (magnitude >= 65520)
        return sign * Infinity;
    let exponent = Math.floor(Math.log2(magnitude));
    if (2 ** exponent > magnitude)
        exponent--;
    if (2 ** (exponent + 1) <= magnitude)
        exponent++;
    let ulp = 2 ** (Math.max(exponent, -14) - 10);
    let scaled = magnitude / ulp;
    let rounded = Math.floor(scaled);
    let fraction = scaled - rounded;
    if (fraction > 0.5 || (fraction === 0.5 && (rounded & 1)))
        rounded++;
    return sign * rounded * ulp;
}

function referenceBits(value) {
    let rounded = referenceRound(value);
    let sign = (rounded < 0 || Object.is(rounded, -0)) ? 0x8000 : 0;
    let magnitude = Math.abs(rounded);
    if (magnitude === Infinity)
        return sign | 0x7c00;
    if (magnitude < 2 ** -14)
        return sign | (magnitude / 2 ** -24);
    let exponent = Math.floor(Math.log2(magnitude));
    if (2 ** exponent > magnitude)
        exponent--;
    if (2 ** (exponent + 1) <= magnitude)
        exponent++;
    let bits = sign | ((exponent + 15) << 10) | (magnitude / 2 ** (exponent - 10) - 1024);
    if (!Object.is(halfBitsToDouble(bits), rounded))
        throw new Error(`bad reference bits for ${value}`);
    return bits;
}

let values = [0, -0, 1, -1, 0.1, 1 / 3, 65504, 65519.99999999999, 65520, 1e10, 2 ** -24, 2 ** -25, 2 ** -25 * (1 + 2 ** -40), 2 ** -26, 1e-300, Infinity, -Infinity, NaN];
for (let bits = 0; bits < 0x7c00; bits += 37) {
    let low = halfBitsToDouble(bits);
    let high = bits === 0x7bff ? 65536 : halfBitsToDouble(bits + 1);
    let middle = (low + high) / 2;
    // Rounding through float32 first turns the perturbed midpoints into exact ties.
    for (let value of [low, middle, middle * (1 + 2 ** -40), middle * (1 - 2 ** -40)]) {
        values.push(value);
        values.push(-value);
    }
}
let expected = values.map(referenceRound);
let expectedBits = values.map(value => Number.isNaN(value) ? -1 : referenceBits(value));

function f16round(value) {
    return Math.f16round(value);
}
noInline(f16round);

function storeAndLoad(array, index, value) {
    array[index] = value;
    return array[index];
}
noInline(storeAndLoad);

function dataViewStore(view, value, littleEndian) {
    view.setFloat16(1, value, littleEndian);
}
noInline(dataViewStore);

function dataViewLoad(view, littleEndian) {
    return view.getFloat16(1, littleEndian);
}
noInline(dataViewLoad);

let array = new Float16Array(4);
let buffer = new ArrayBuffer(4);
let view = new DataView(buffer);
let bytes = new Uint8Array(buffer);

for (let iteration = 0; iteration < 10; ++iteration) {
    for (let i = 0; i < values.length; ++i) {
        let value = values[i];
        shouldBe(f16round(value), expected[i], "Math.f16round", value);
        shouldBe(storeAndLoad(array, i & 3, value), expected[i], "Float16Array store and load", value);

        for (let littleEndian of [true, false]) {
            dataViewStore(view, value, littleEndian);
            let bits = littleEndian ? (bytes[1] | (bytes[2] << 8)) : ((bytes[1] << 8) | bytes[2]);
            if (expectedBits[i] === -1)
                shouldBe(Number.isNaN(halfBitsToDouble(bits)), true, "DataView setFloat16 is NaN", value, littleEndian);
            else
                shouldBe(bits, expectedBits[i], "DataView setFloat16 bits", value, littleEndian);
            shouldBe(dataViewLoad(view, littleEndian), expected[i], "DataView getFloat16 after setFloat16", value, littleEndian);
        }
    }
}

// Widening a half NaN keeps its sign and payload, so 0xffff becomes a double in the impure NaN range,
// which must never be boxed as is.
function load(array, index) {
    return array[index];
}
noInline(load);

function loadAndBox(array, index) {
    let value = array[index];
    return [value, { value }];
}
noInline(loadAndBox);

function shouldBeBoxedNaN(actual, operation, bits) {
    if (typeof actual !== "number" || !Number.isNaN(actual))
        throw new Error(`bad value for ${operation} of 0x${bits.toString(16)}: ${String(actual)}`);
}

let nanBits = [0x7c01, 0x7d55, 0x7e00, 0x7fff, 0xfc01, 0xfd55, 0xfe00, 0xffff];
let nanArray = new Float16Array(nanBits.length);
let nanHalves = new Uint16Array(nanArray.buffer);
nanHalves.set(nanBits);
let nanView = new DataView(new ArrayBuffer(4));
let nanBytes = new Uint8Array(nanView.buffer);

for (let iteration = 0; iteration < 10000; ++iteration) {
    for (let i = 0; i < nanBits.length; ++i) {
        let bits = nanBits[i];
        shouldBeBoxedNaN(load(nanArray, i), "Float16Array load", bits);
        let [element, holder] = loadAndBox(nanArray, i);
        shouldBeBoxedNaN(element, "Float16Array load stored in an array", bits);
        shouldBeBoxedNaN(holder.value, "Float16Array load stored in an object", bits);

        for (let littleEndian of [true, false]) {
            nanBytes[1] = littleEndian ? bits & 0xff : bits >> 8;
            nanBytes[2] = littleEndian ? bits >> 8 : bits & 0xff;
            shouldBeBoxedNaN(dataViewLoad(nanView, littleEndian), "DataView getFloat16", bits);
        }
    }
}
