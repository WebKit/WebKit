function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array1) {
    return array1.concat();
}
noInline(test);

var array1 = [2, 3, 4, 5];
for (var i = 0; i < 1e4; ++i) {
    var result = test(array1);
    shouldBe(result[0], 2);
    shouldBe(result[1], 3);
    shouldBe(result[2], 4);
    shouldBe(result[3], 5);
    shouldBe(result.length, 4);
}
