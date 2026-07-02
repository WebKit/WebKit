// The original is https://github.com/tc39/test262/blob/d0c1b4555b03dd404873fd6422a4b5da00136500/test/staging/sm/TypedArray/constructor-buffer-sequence.js

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
    Int8Array,
    Uint16Array,
    Uint32Array,
    Uint8Array,
    Uint8ClampedArray,
];
for (const Ctor of TEST_TARGET) {
    const label = Ctor.name;

    // if the step 2 |ToIndex(byteOffset)| detaches the array buffer,
    // the step 6 |IsDetachedBuffer(buffer)| should catch it.
    // https://tc39.es/ecma262/2026/#sec-initializetypedarrayfromarraybuffer
    for (const { buffer, detach } of createBuffers()) {
        const byteOffset = ValueReturning(0, detach);

        shouldThrow(label, () => {
            new Ctor(buffer, byteOffset, 0)
        }, TypeError, 'Buffer is already detached');
    }
}