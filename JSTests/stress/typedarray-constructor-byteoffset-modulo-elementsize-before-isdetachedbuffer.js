// The original is https://github.com/tc39/test262/blob/d0c1b4555b03dd404873fd6422a4b5da00136500/test/staging/sm/TypedArray/constructor-buffer-sequence.js

function assert(ok, message = '') {
    if (!ok)
        throw new Error(`Assertion!: ${message}`); 
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

const otherGlobal = $.createRealm().global;

function* createBuffers(lengths = [0, 8]) {
    for (const length of lengths) {
        const buffer = new ArrayBuffer(length);
        yield {
            buffer,
            detach: () => $.detachArrayBuffer(buffer),
        };
    }

    for (const length of lengths) {
        const buffer = new otherGlobal.ArrayBuffer(length);
        yield {
            buffer,
            detach: () => otherGlobal.$.detachArrayBuffer(buffer),
        };
    }
}

function ValueReturning(value, detach) {
    return {
        valueOf() {
            if (detach)
                detach();
            return value;
        }
    };
}

const TEST_TARGET = [
    BigInt64Array,
    BigUint64Array,
    Float16Array,
    Float32Array,
    Float64Array,
    Int16Array,
    Int32Array,
    // Int8Array,
    Uint16Array,
    Uint32Array,
    // Uint8Array,
    // Uint8ClampedArray,
];

for (const Ctor of TEST_TARGET) {
    const label = Ctor.name;

    assert(Ctor.BYTES_PER_ELEMENT !== 1, `${label}: |byteOffset % 1| is always 0. We cannot test what we want to test`);

    for (const { buffer, detach } of createBuffers()) {
        detach();

        // https://tc39.es/ecma262/2026/#sec-initializetypedarrayfromarraybuffer
        //
        // the step 3 |offset modulo elementSize ≠ 0| should be executed before the step 6 |IsDetachedBuffer(buffer)|.
        shouldThrow(label, () => {
            new Ctor(buffer, 1, 0);
        }, RangeError, "byteOffset modulo TypedArray.BYTES_PER_ELEMENT must be 0");
    }
}