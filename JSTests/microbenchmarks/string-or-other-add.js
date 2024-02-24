function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(value) {
    return value + '';
}
noInline(test);

var string = "hello world";
for (var i = 0; i < 1e7; ++i) {
    shouldBe(test(string), "hello world");
    shouldBe(test(null), "null");
    shouldBe(test(undefined), "undefined");
}
