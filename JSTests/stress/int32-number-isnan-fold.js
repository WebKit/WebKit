function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, index) {
    return Number.isNaN(array[index]);
}
noInline(test);

var array = ['string', 0, 1, 2, 3, NaN];
for (var i = 0; i < 1e6; ++i) {
    shouldBe(test(array, 1), false);
    shouldBe(test(array, 2), false);
    shouldBe(test(array, 3), false);
}
shouldBe(test(array, 5), true);
