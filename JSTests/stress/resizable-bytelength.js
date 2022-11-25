//@ requireOptions("--useResizableArrayBuffer=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    var buffer = new ArrayBuffer(0, { maxByteLength: 1024 });
    var array = new Uint8Array(buffer);
    for (var i = 0; i < 1024; ++i) {
        buffer.resize(i);
        shouldBe(array.byteLength, i);
    }
}

for (var i = 0; i < 1e4; ++i)
    test();
