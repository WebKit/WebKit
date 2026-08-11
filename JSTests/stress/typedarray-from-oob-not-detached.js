function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected: ${expected}`);
}

// Source is a fixed-length view on a resizable ArrayBuffer. Shrinking the
// buffer inside mapFn makes the view out-of-bounds without detaching it.
// TypedArray.from must stop writing without throwing.
function testSourceShrunkOutOfBounds() {
    const rab = new ArrayBuffer(32, { maxByteLength: 64 });
    const source = new Int32Array(rab, 0, 8);
    for (let i = 0; i < 8; i++)
        source[i] = i + 1;
    source[Symbol.iterator] = null;

    let calls = 0;
    const result = Int32Array.from(source, (value, k) => {
        calls++;
        if (k === 4)
            rab.resize(16);
        return value;
    });
    shouldBe(calls, 5);
    shouldBe(result.length, 8);
    shouldBe(result[4], 5);
    shouldBe(result[5], 0);
}

// Result is a fixed-length view on a resizable ArrayBuffer. Shrinking the
// buffer inside mapFn makes the result out-of-bounds without detaching it.
function testResultShrunkOutOfBounds() {
    let rab;
    class FixedLengthOnResizable extends Int32Array {
        constructor(length) {
            rab = new ArrayBuffer(4 * length, { maxByteLength: 8 * length });
            super(rab, 0, length);
        }
    }

    let calls = 0;
    const result = FixedLengthOnResizable.from({ length: 8 }, (value, k) => {
        calls++;
        if (k === 2)
            rab.resize(4);
        return k;
    });
    shouldBe(calls, 3);
}

// Result's buffer is detached by transfer() inside mapFn.
function testResultDetached() {
    let buffer;
    class ViewOnTransferableBuffer extends Int32Array {
        constructor(length) {
            buffer = new ArrayBuffer(4 * length);
            super(buffer);
        }
    }

    let calls = 0;
    const result = ViewOnTransferableBuffer.from({ length: 8 }, (value, k) => {
        calls++;
        if (k === 2)
            buffer.transfer();
        return k;
    });
    shouldBe(calls, 3);
}

for (let i = 0; i < testLoopCount; i++) {
    testSourceShrunkOutOfBounds();
    testResultShrunkOutOfBounds();
    testResultDetached();
}
