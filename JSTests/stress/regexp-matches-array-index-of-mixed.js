function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

var regExp = /(x)?(y)(z)?/;

function indexOfValue(array, value) {
    return array.indexOf(value, 1);
}
noInline(indexOfValue);

for (var i = 0; i < 1e5; ++i) {
    var match = 'y'.match(regExp);
    shouldBe(indexOfValue(match, 'y'), 2);
    shouldBe(indexOfValue(match, undefined), 1);

    var plain = ['y', undefined, 'y', undefined];
    shouldBe(indexOfValue(plain, 'y'), 2);
    shouldBe(indexOfValue(plain, undefined), 1);

    var decorated = ['y', undefined, 'y', undefined];
    decorated.index = 0;
    decorated.input = 'y';
    shouldBe(indexOfValue(decorated, 'y'), 2);
    shouldBe(indexOfValue(decorated, undefined), 1);
}
