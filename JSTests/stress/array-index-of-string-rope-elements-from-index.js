function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function makeRope(a, b) {
    return a + b;
}
noInline(makeRope);

function indexOfValueFrom(array, value, from) {
    return array.indexOf(value, from);
}
noInline(indexOfValueFrom);

for (var i = 0; i < 2e5; ++i) {
    var array = [
        makeRope('ab', 'cd'),
        makeRope('zz', 'zz'),
        makeRope('ab', 'cd'),
    ];
    shouldBe(indexOfValueFrom(array, 'abcd', 0), 0);
    shouldBe(indexOfValueFrom(array, 'abcd', 1), 2);
    shouldBe(indexOfValueFrom(array, 'abcd', -1), 2);
    shouldBe(indexOfValueFrom(array, 'zzzz', 2), -1);
    shouldBe(indexOfValueFrom(array, 'zzzz', 1), 1);
}
