function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

function indexOfValue(array, value) {
    return array.indexOf(value);
}
noInline(indexOfValue);

for (var i = 0; i < 2e5; ++i) {
    var array = [
        makeRope('あい', 'う'),
        makeRope('か', 'き'),
        'あいう',
        '',
    ];
    shouldBe(indexOfValue(array, ''), 3);
    shouldBe(indexOfValue(array, 'あいう'), 0);
    shouldBe(indexOfValue(array, 'かき'), 1);
    shouldBe(indexOfValue(array, 'あいか'), -1);

    var mixed = [makeRope('あい', 'う'), makeRope('ab', 'cd'), 'x'];
    shouldBe(indexOfValue(mixed, 'x'), 2);
    shouldBe(indexOfValue(mixed, 'abcd'), 1);
}
