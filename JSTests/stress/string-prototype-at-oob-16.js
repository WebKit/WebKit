function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string, index) {
    return string.at(index);
}
noInline(test);

var string = "こんにちは、世界";
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(string, 0), 'こ');
    shouldBe(test(string, 1), 'ん');
    shouldBe(test(string, 3), 'ち');
    shouldBe(test(string, -2), '世');
    shouldBe(test(string, 42), undefined);
    shouldBe(test(string, -20), undefined);
}
