function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array, index) {
    return array.at(index);
}
noInline(test);

var array = [12, 4, 4, 66, 56, 44, 44, 38, 65, 89, 2, 45, 789, 56, 432, 677, 2234, 77, 3457, 133, 6544, 76, 17, 322, 34];
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(array, 0), 12);
    shouldBe(test(array, 1), 4);
    shouldBe(test(array, 3), 66);
    shouldBe(test(array, -2), 322);
    shouldBe(test(array, 42), undefined);
    shouldBe(test(array, -40), undefined);
}

