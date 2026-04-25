function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b) {
    return a < b;
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(null, 0.2), true);
    shouldBe(test(0.5, 0.2), false);
    shouldBe(test(0.5, undefined), false);
}

shouldBe(test(1, 2), true);
for (var i = 0; i < 1e3; ++i)
    shouldBe(test(1n, 2), true);
