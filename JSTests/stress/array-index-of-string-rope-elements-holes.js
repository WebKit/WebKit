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
    var holey = [makeRope('ab', 'cd')];
    holey[3] = '';
    shouldBe(indexOfValue(holey, ''), 3);
    shouldBe(indexOfValue(holey, 'abcd'), 0);
    shouldBe(indexOfValue(holey, undefined), -1);
}
