//@ requireOptions("--useResizableArrayBuffer=1")
function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var buf = new ArrayBuffer(42, { maxByteLength: 128 << 10 });
shouldBe(buf.resizable, true);
shouldBe(buf.byteLength, 42);
buf.resize(0);
shouldBe(buf.byteLength, 0);
buf.resize(32 << 10);
shouldBe(buf.byteLength, 32 << 10);
