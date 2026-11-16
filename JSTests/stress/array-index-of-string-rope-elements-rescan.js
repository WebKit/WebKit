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
    var array = [makeRope('zz', 'zz'), 'abcd', makeRope('ab', 'cd')];
    shouldBe(indexOfValue(array, 'abcd'), 1);
    shouldBe(includesValue(array, 'abcd'), true);

    var array2 = [makeRope('ab', 'cd'), 'abcd'];
    shouldBe(indexOfValue(array2, 'abcd'), 0);
}
