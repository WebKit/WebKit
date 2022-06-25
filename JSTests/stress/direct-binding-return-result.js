function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test() {
    var a, b;
    return ([a, b] = [1, 2]);
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var result = test();
    shouldBe(result.length, 2);
    shouldBe(result[0], 1);
    shouldBe(result[1], 2);
}
