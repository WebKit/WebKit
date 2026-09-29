function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function hash(value, r) {
    return ((value + r) * 52845 + 22719) & 0xffff;
}
noInline(hash);

function product(a, b) {
    return a * b;
}
noInline(product);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(hash(i & 0xff, (i * 7) & 0xffff), (((i & 0xff) + ((i * 7) & 0xffff)) * 52845 + 22719) & 0xffff);
    shouldBe(product(i + 100000, 100000), (i + 100000) * 100000);
}

// Leaves the Int52 range.
shouldBe(product(0x7fffffff, 0x7fffffff), 4611686014132420609);
shouldBe(product(-0x80000000, 0x7fffffff), -4611686016279904256);
shouldBe(hash(0x7fffffff, 0x7fffffff), ((0x7fffffff + 0x7fffffff) * 52845 + 22719) & 0xffff);
shouldBe(product(0, -5), -0);
shouldBe(product(-5, 0), -0);
shouldBe(product(1.5, 3), 4.5);
shouldBe(product(100000, 100000), 10000000000);
shouldBe(product(-0x80000000, 0x100000), -2251799813685248);
shouldBe(product(0x80000000, 0x100000), 2251799813685248);
shouldBe(product(-0x80000000, 0x100001), -2251801961168896);
