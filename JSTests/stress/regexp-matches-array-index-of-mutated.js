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
    match[3] = 'replaced';
    shouldBe(indexOfValue(match, 'replaced'), 3);
    shouldBe(indexOfValue(match, undefined), 1);

    var extended = 'y'.match(regExp);
    extended.extra = 42;
    shouldBe(indexOfValue(extended, 'y'), 2);

    var grown = 'y'.match(regExp);
    grown.length = 10;
    shouldBe(indexOfValue(grown, 'y'), 2);
    shouldBe(indexOfValue(grown, undefined), 1);

    var holey = 'y'.match(regExp);
    holey[8] = 'tail';
    shouldBe(indexOfValue(holey, 'tail'), 8);
    shouldBe(indexOfValue(holey, undefined), 1);
}
