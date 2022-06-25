function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(i8, double) {
    shouldBe(Math.sign(Atomics.store(i8, 0, double)), 0);
}
noInline(test);

var i8 = new Int8Array(new SharedArrayBuffer(8));
for (var i = 0; i < 1e4; ++i)
    test(i8, -0.0);
