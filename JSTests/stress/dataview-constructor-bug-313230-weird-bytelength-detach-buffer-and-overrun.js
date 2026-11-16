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
    DataView,
];

for (const targetCtor of TEST_TARGET) {
    const name = targetCtor.name;

    const buffer = new ArrayBuffer(4096);
    const byteOffset = 2048;
    const actualByteLength = 2048 + 1;
    // https://tc39.es/ecma262/#sec-dataview-buffer-byteoffset-bytelength
    assert((byteOffset + actualByteLength ) > buffer.byteLength, `25.3.2.1, step 9-b's condition not satisfied`);

    const byteLength = {
        valueOf: function () {
            $.detachArrayBuffer(buffer);
            $.gc();
            return actualByteLength;
        }
    };

    // By the spec (April 24, 2026),
    // https://tc39.es/ecma262/#sec-dataview-buffer-byteoffset-bytelength defines:
    //
    //  1. The step 3 get _offset_ by ToIndex(byteOffset).
    //      - We fix that in bug 311903 if the byteOffset is weird and this steps detachs the buffer.
    //  2. Check IsDetachedBuffer(buffer), but ok.
    //  3. The step 9-a get _viewByteLength_ by ToIndex(byteLength).
    //      - This detach the buffer.
    //  4. The step 9-b check whether `offset + viewByteLength > bufferByteLength` and throw RangeError.
    shouldThrow(`${name}: should throw as the expected`, () => {
        new targetCtor(buffer, byteOffset, byteLength);
    }, RangeError, 'Length out of range of buffer');
    sameValue(buffer.detached, true, `${name}: arrayBuffer is detached as expectedly`);

    // The detached ArrayBuffer.byteLength should be set to 0.
    //
    //  - https://tc39.es/ecma262/#sec-detacharraybuffer
    //  - https://tc39.es/ecma262/#sec-get-arraybuffer.prototype.bytelength
    sameValue(buffer.byteLength, 0, `${name}: arrayBuffer.byteLength is 0`);
}