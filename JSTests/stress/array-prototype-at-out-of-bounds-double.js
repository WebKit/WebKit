function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, index) {
    return array.at(index);
}
noInline(test);

var array = [12.5, 4.5, 4.5, 66.5, 56.5, 44.5, 44.5, 38.5, 65.5, 89.5, 2.5, 45.5, 789.5, 56.5, 432.5, 677.5, 2234.5, 77.5, 3457.5, 133.5, 6544.5, 76.5, 17.5, 322.5, 34.5];
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(array, 0), 12.5);
    shouldBe(test(array, 1), 4.5);
    shouldBe(test(array, 3), 66.5);
    shouldBe(test(array, -2), 322.5);
    shouldBe(test(array, 42), undefined);
    shouldBe(test(array, -40), undefined);
}

