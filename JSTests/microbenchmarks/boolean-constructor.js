function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return Boolean(string);
}
noInline(test);

var object = {};
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(""), false);
    shouldBe(test("240"), true);
    shouldBe(test(0), false);
    shouldBe(test(-0), false);
    shouldBe(test(1), true);
    shouldBe(test(test), true);
    shouldBe(test(object), true);
}
