function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array1, array2) {
    return array1.concat(array2);
}
noInline(test);

var array1 = [0, 1, 2, 3, 4]
var array2 = [2.1, 2.2, 2.3, 2.4];
for (var i = 0; i < 1e4; ++i) {
    var result = test(array1, array2);
    shouldBe(result[0], 0);
    shouldBe(result[1], 1);
    shouldBe(result[2], 2);
    shouldBe(result[3], 3);
    shouldBe(result[4], 4);
    shouldBe(result[5], 2.1);
    shouldBe(result[6], 2.2);
    shouldBe(result[7], 2.3);
    shouldBe(result[8], 2.4);
    shouldBe(result.length, 9);
}
