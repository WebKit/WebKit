function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

var regExp = /a(b)?(c)?/d;

function indexOfValue(array, value) {
    return array.indexOf(value);
}
noInline(indexOfValue);

function includesValue(array, value) {
    return array.includes(value);
}
noInline(includesValue);

for (var i = 0; i < 1e5; ++i) {
    var match = 'ac'.match(regExp);
    shouldBe(indexOfValue(match, 'c'), 2);
    shouldBe(indexOfValue(match, undefined), 1);
    shouldBe(indexOfValue(match, 'missing'), -1);
    shouldBe(includesValue(match, 'c'), true);
    shouldBe(includesValue(match, 'missing'), false);
}
