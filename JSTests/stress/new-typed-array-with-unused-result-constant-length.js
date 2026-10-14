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

function newUnusedWithConstantLength() {
    new Uint8Array(16);
    new Float64Array(0);
    new ArrayBuffer(32);
    return 1;
}
noInline(newUnusedWithConstantLength);

function newUint8ArrayWithNegativeConstant() {
    new Uint8Array(-1);
    return 1;
}
noInline(newUint8ArrayWithNegativeConstant);

function newArrayBufferWithNegativeConstant() {
    new ArrayBuffer(-1);
    return 1;
}
noInline(newArrayBufferWithNegativeConstant);

for (let i = 0; i < testLoopCount; i++) {
    if (newUnusedWithConstantLength() !== 1)
        throw new Error("bad result");
    shouldThrowRangeError(newUint8ArrayWithNegativeConstant, "new Uint8Array(-1)");
    shouldThrowRangeError(newArrayBufferWithNegativeConstant, "new ArrayBuffer(-1)");
}
