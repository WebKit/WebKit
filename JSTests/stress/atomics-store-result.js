function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var i8 = new Int8Array(32);
for (var i = 0; i < 1e4; ++i)
    shouldBe(Atomics.store(i8, 0, 0xff00 + i), 0xff00 + i);
