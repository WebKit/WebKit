//@ requireOptions("--useResizableArrayBuffer=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

{
    let buffer = new SharedArrayBuffer(42, { maxByteLength: 1024 });
    let array = new Int8Array(buffer);
    shouldBe(array.length, 42);
    shouldBe(array.byteLength, 42);
    buffer.grow(128);
    shouldBe(array.length, 128);
    shouldBe(array.byteLength, 128);
    buffer.grow(1024);
    shouldBe(array.length, 1024);
    shouldBe(array.byteLength, 1024);
}

{
    let buffer = new SharedArrayBuffer(42, { maxByteLength: 1024 });
    let view = new DataView(buffer);
    shouldBe(view.byteLength, 42);
    buffer.grow(128);
    shouldBe(view.byteLength, 128);
    buffer.grow(1024);
    shouldBe(view.byteLength, 1024);
}

{
    let buffer = new SharedArrayBuffer(42, { maxByteLength: 1024 });
    shouldThrow(() => {
        let array = new Int8Array(buffer, 128);
    }, `RangeError: byteOffset exceeds source ArrayBuffer byteLength`);
    let array = new Int8Array(buffer, 16);
    shouldBe(array.length, 26);
    shouldBe(array.byteLength, 26);
    buffer.grow(128);
    shouldBe(array.length, 112);
    shouldBe(array.byteLength, 112);
    buffer.grow(1024);
    shouldBe(array.length, 1008);
    shouldBe(array.byteLength, 1008);
}

{
    let buffer = new SharedArrayBuffer(42, { maxByteLength: 1024 });
    shouldThrow(() => {
        let view = new DataView(buffer, 128);
    }, `RangeError: byteOffset exceeds source ArrayBuffer byteLength`);
    let view = new DataView(buffer, 16);
    shouldBe(view.byteLength, 26);
    buffer.grow(128);
    shouldBe(view.byteLength, 112);
    buffer.grow(1024);
    shouldBe(view.byteLength, 1008);
}
