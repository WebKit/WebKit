function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(a, b)
{
    return a === b;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (i & 1) {
        test(0n, 0n);
        test(0n, 10n);
    } else {
        test(100000000000000000000000000000000000000000n, 200n);
        test(100000000000000000000000000000000000000000n, 100000000000000000000000000000000000000000n);
    }
}
shouldBe(test(20, 20), true);
shouldBe(test(20, 40), false);
shouldBe(test(0.5, 0.5), true);
shouldBe(test(0.5, 0.24), false);
shouldBe(test(20.1 - 0.1, 20), true);
