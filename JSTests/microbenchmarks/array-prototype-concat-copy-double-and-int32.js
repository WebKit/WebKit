function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array1) {
    return array1.concat();
}
noInline(test);

var array1 = [2.1, 2.2, 2.3, 2.4, 4, 5, 6, 7];
for (var i = 0; i < 1e4; ++i) {
    var result = test(array1);
    shouldBe(result[0], 2.1);
    shouldBe(result[1], 2.2);
    shouldBe(result[2], 2.3);
    shouldBe(result[3], 2.4);
    shouldBe(result[4], 4);
    shouldBe(result[5], 5);
    shouldBe(result[6], 6);
    shouldBe(result[7], 7);
    shouldBe(result.length, 8);
}
