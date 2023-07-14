function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(value) {
    return String.fromCharCode(value);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    shouldBe(test(i + 0.7), String.fromCharCode(i));
