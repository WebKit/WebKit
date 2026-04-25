function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(i8, double) {
    shouldBe(Atomics.store(i8, 0, double), double);
}
noInline(test);

var i8 = new Int8Array(32);
for (var i = 0; i < 1e4; ++i) {
    test(i8, Infinity);
    test(i8, -Infinity);
}
