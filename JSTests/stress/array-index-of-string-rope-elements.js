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

function includesValue(array, value) {
    return array.includes(value);
}
noInline(includesValue);

for (var i = 0; i < 2e5; ++i) {
    var array = [
        makeRope('ab', 'cd'),
        makeRope('wx', 'yz'),
        'hello',
        undefined,
        makeRope('a', 'b'),
        '',
    ];

    shouldBe(indexOfValue(array, ''), 5);
    shouldBe(indexOfValue(array, 'ab'), 4);
    shouldBe(includesValue(array, ''), true);

    shouldBe(indexOfValue(array, 'abcd'), 0);
    shouldBe(indexOfValue(array, 'wxyz'), 1);
    shouldBe(indexOfValue(array, 'xyzw'), -1);
    shouldBe(includesValue(array, 'wxyz'), true);
    shouldBe(includesValue(array, 'xyzw'), false);

    shouldBe(indexOfValue(array, 'hello'), 2);
    shouldBe(indexOfValue(array, undefined), 3);
}
