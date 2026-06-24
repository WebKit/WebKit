function makeUint8(length) { return new Uint8Array(length); }
noInline(makeUint8);
function makeInt16(length) { return new Int16Array(length); }
noInline(makeInt16);
function makeInt32(length) { return new Int32Array(length); }
noInline(makeInt32);
function makeFloat32(length) { return new Float32Array(length); }
noInline(makeFloat32);
function makeFloat64(length) { return new Float64Array(length); }
noInline(makeFloat64);
function makeBigInt64(length) { return new BigInt64Array(length); }
noInline(makeBigInt64);
function makeConstantSizes() { return [new Float64Array(16), new Float64Array(17), new Uint8Array(128), new Uint8Array(129), new Int32Array(512), new Float64Array(1000)]; }
noInline(makeConstantSizes);

function check(array, length, zero) {
    if (array.length !== length)
        throw new Error("bad length " + array.length + ", expected " + length);
    for (let i = 0; i < length; ++i) {
        if (array[i] !== zero)
            throw new Error("non-zero element " + array[i] + " at index " + i + " of length " + length);
    }
}

const lengths = [0, 1, 2, 3, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, 255, 256, 257, 511, 512, 513, 999, 1000, 1001];
const constantLengths = [16, 17, 128, 129, 512, 1000];

for (let i = 0; i < testLoopCount; ++i) {
    let length = lengths[i % lengths.length];
    check(makeUint8(length), length, 0);
    check(makeInt16(length), length, 0);
    check(makeInt32(length), length, 0);
    check(makeFloat32(length), length, 0);
    check(makeFloat64(length), length, 0);
    check(makeBigInt64(length), length, 0n);
    let constants = makeConstantSizes();
    for (let j = 0; j < constants.length; ++j)
        check(constants[j], constantLengths[j], 0);
}
