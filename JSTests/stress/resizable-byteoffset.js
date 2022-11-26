//@ requireOptions("--useResizableArrayBuffer=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    var buffer = new ArrayBuffer(128, { maxByteLength: 1024 });
    var array = new Uint8Array(buffer, 64);
    for (var i = 0; i < 1024; ++i) {
        buffer.resize(i);
        if (i < 64)
            shouldBe(array.byteOffset, 0);
        else
            shouldBe(array.byteOffset, 64);
    }
}

for (var i = 0; i < 1e4; ++i)
    test();
