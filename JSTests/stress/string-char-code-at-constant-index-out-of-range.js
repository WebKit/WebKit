function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return string.charCodeAt(0);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(Number.isNaN(test("")), true);
