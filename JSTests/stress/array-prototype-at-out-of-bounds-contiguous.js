function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error('bad value: ' + JSON.stringify(actual));
}

function test(array, index) {
    return array.at(index);
}
noInline(test);

var array = [
    { value: 12 }, { value: 4 }, { value: 4 }, { value: 66 }, { value: 56 },
    { value: 44 }, { value: 44 }, { value: 38 }, { value: 65 }, { value: 89 },
    { value: 2 }, { value: 45 }, { value: 789 }, { value: 56 }, { value: 432 },
    { value: 677 }, { value: 2234 }, { value: 77 }, { value: 3457 }, { value: 133 },
    { value: 6544 }, { value: 76 }, { value: 17 }, { value: 322 }, { value: 34 }
];
for (var i = 0; i < 1e4; ++i) {
    shouldBe(test(array, 0), { value: 12 });
    shouldBe(test(array, 1), { value: 4 });
    shouldBe(test(array, 3), { value: 66 });
    shouldBe(test(array, -2), { value: 322 });
    shouldBe(test(array, 42), undefined);
    shouldBe(test(array, -40), undefined);
}

