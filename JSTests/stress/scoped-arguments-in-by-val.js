function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b, c) {
    (function () {
        a = 42;
    }());
    shouldBe(0 in arguments, true);
}

for (var i = 0; i < 1e6; ++i)
    test(0, 1, 2);
