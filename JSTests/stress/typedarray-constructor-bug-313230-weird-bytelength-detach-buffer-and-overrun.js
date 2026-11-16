function assert(ok, message = '') {
    if (!ok)
        throw new Error(`Assertion!: ${message}`); 
}

function sameValue(a, b, testname) {
    if (a !== b)
        throw new Error(`${testname}: Expected ${b} but got ${a}`);
}

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

for (const targetCtor of TEST_TARGET) {
    const name = targetCtor.name;

    const buffer = new ArrayBuffer(4096);
    const byteOffset = 2048;
    const actualByteLength = 2048 + 1;
    // https://tc39.es/ecma262/#sec-initializetypedarrayfromarraybuffer
    assert((byteOffset + actualByteLength ) > buffer.byteLength, `23.2.5.1.3, step 9-b-2's condition not satisfied`);

    const byteLength = {
        valueOf: function () {
            $.detachArrayBuffer(buffer);
            $.gc();
            return actualByteLength;
        }
    };

    // By the spec (April 24, 2026),
    // https://tc39.es/ecma262/#sec-initializetypedarrayfromarraybuffer,
    // which is invoked by the step 7-c of https://tc39.es/ecma262/#sec-typedarray, defines:
    //
    //  1. The step 3 get _offset_ by ToIndex(byteOffset).
    //      - The weird `byteOffset ` detach the buffer here.
    //  2. The step 7 check IsDetachedBuffer(buffer) and throw TypeError.
    //  3. The step 9-b-2 checks whether `offset + newByteLength > bufferByteLength`,
    //     but not reached to here by the step 7 in contrast to DataView()'s similar pattern.
    shouldThrow(`${name}: should throw as the expected`, () => {
        new targetCtor(buffer, byteOffset, byteLength);
    }, TypeError, 'Buffer is already detached');
    sameValue(buffer.detached, true, `${name}: arrayBuffer is detached as expectedly`);

    // The detached ArrayBuffer.byteLength should be set to 0.
    //
    //  - https://tc39.es/ecma262/#sec-detacharraybuffer
    //  - https://tc39.es/ecma262/#sec-get-arraybuffer.prototype.bytelength
    sameValue(buffer.byteLength, 0, `${name}: arrayBuffer.byteLength is 0`);
}