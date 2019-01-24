function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(value)
{
    return value[2];
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(test("Hello"), 'l');
    shouldBe(test("World"), 'r');
    shouldBe(test("Nice"), 'c');
}
