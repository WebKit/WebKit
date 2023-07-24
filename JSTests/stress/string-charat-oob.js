function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string, index) {
    return string.charAt(index);
}
noInline(test);

var string = "Hello";
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(string, 0), 'H');
    shouldBe(test(string, 1), 'e');
    shouldBe(test(string, 3), 'l');
    shouldBe(test(string, 42), '');
    shouldBe(test(string, -20), '');
}
