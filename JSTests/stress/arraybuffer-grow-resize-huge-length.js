function shouldThrowRangeError(func)
{
    var threw = false;
    try {
        func();
    } catch (error) {
        threw = true;
        if (!(error instanceof RangeError))
            throw new Error("expected RangeError, got " + error);
    }
    if (!threw)
        throw new Error("did not throw");
}

{
    let buffer = new ArrayBuffer(8, { maxByteLength: 16 });
    shouldThrowRangeError(() => buffer.resize(1e20));
    shouldThrowRangeError(() => buffer.resize(Infinity));
    shouldThrowRangeError(() => buffer.resize(-1));
    let detached = new ArrayBuffer(8, { maxByteLength: 16 });
    detached.transfer();
    shouldThrowRangeError(() => detached.resize(-1));
    buffer.resize(16);
    if (buffer.byteLength !== 16)
        throw new Error("resize to 16 failed");
}

{
    let buffer = new SharedArrayBuffer(8, { maxByteLength: 16 });
    shouldThrowRangeError(() => buffer.grow(1e20));
    shouldThrowRangeError(() => buffer.grow(Infinity));
    shouldThrowRangeError(() => buffer.grow(-1));
    buffer.grow(16);
    if (buffer.byteLength !== 16)
        throw new Error("grow to 16 failed");
}
