function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return Number(string);
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    shouldBe(test("-0"), 0);
    shouldBe((1 / test("-0")) < 0, true);
    shouldBe(test("0"), 0);
    shouldBe(test("1"), 1);
    shouldBe(test("240"), 240);
    shouldBe(test("-1"), -1);
}
