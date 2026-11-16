function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

function errorFrom(func) {
    try {
        func();
    } catch (e) {
        return e;
    }
    return null;
}

// A negative length speculated as an int32 must throw the same RangeError as the generic path.

function newArrayBufferUnused(n) {
    new ArrayBuffer(n);
    return 1;
}
noInline(newArrayBufferUnused);

function newArrayBuffer(n) {
    return new ArrayBuffer(n);
}
noInline(newArrayBuffer);

for (let i = 0; i < testLoopCount / 4; ++i) {
    newArrayBufferUnused(i & 7);
    newArrayBuffer(i & 7);
}

for (let func of [newArrayBufferUnused, newArrayBuffer]) {
    let error = errorFrom(() => func(-1));
    shouldBe(error instanceof RangeError, true, func.name + " error type");
    shouldBe(error.message, "length cannot be negative", func.name + " error message");
}
