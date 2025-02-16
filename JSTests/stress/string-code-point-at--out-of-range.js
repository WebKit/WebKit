function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string, index) {
    return string.codePointAt(index);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test("", -1), undefined);
