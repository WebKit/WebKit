function shouldThrowRangeError(func, what) {
    let error = null;
    try {
        func();
    } catch (e) {
        error = e;
    }
    if (!(error instanceof RangeError))
        throw new Error(what + " should throw a RangeError, got " + error);
}

// A negative length throws a RangeError, so the allocation must happen even when its result is unused.

function newUint8Array(n) {
    new Uint8Array(n);
    return 1;
}
noInline(newUint8Array);

function newFloat64Array(n) {
    new Float64Array(n);
    return 1;
}
noInline(newFloat64Array);

function newArrayBuffer(n) {
    new ArrayBuffer(n);
    return 1;
}
noInline(newArrayBuffer);

for (let i = 0; i < testLoopCount; i++) {
    newUint8Array(i & 7);
    newFloat64Array(i & 7);
    newArrayBuffer(i & 7);
}

shouldThrowRangeError(() => newUint8Array(-1), "new Uint8Array(-1)");
shouldThrowRangeError(() => newFloat64Array(-1), "new Float64Array(-1)");
shouldThrowRangeError(() => newArrayBuffer(-1), "new ArrayBuffer(-1)");
