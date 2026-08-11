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
    const buffer = new ArrayBuffer(0x1000);
    const byteOffset = {
        valueOf: function () {
            $.detachArrayBuffer(buffer);
            $.gc();
            return 0x800;
        }
    };

    //  By the spec (ECMA-262/April 10, 2026),
    //  This behavior is deduced from the step 6 of
    //  the section [23.2.5.1.3 InitializeTypedArrayFromArrayBuffer ( O, buffer, byteOffset, length )][initializetypedarrayfromarraybuffer],
    //  which is called from the step 7-c-iii of the section [23.2.5.1 TypedArray ( ...args )][typedarray].
    //
    //  [initializetypedarrayfromarraybuffer]: https://tc39.es/ecma262/#sec-initializetypedarrayfromarraybuffer
    //  [typedarray]: https://tc39.es/ecma262/#sec-typedarray
    shouldThrow(`${name}: should throw as the arrayBuffer is detached`, () => {
        new targetCtor(buffer, byteOffset);
    }, TypeError, 'Buffer is already detached');
    sameValue(buffer.detached, true, `${name}: arrayBuffer is detached as expectedly`);
    // The detached ArrayBuffer.byteLength should be set to 0.
    //
    //  - https://tc39.es/ecma262/#sec-detacharraybuffer
    //  - https://tc39.es/ecma262/#sec-get-arraybuffer.prototype.bytelength
    sameValue(buffer.byteLength, 0, `${name}: arrayBuffer.byteLength is 0`);
}