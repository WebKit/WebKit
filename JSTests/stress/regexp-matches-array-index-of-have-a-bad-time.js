//@ requireOptions("--useDollarVM=1")

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
}

$vm.haveABadTime();

for (var i = 0; i < 1e5; ++i) {
    var match = 'y'.match(regExp);
    shouldBe(indexOfValue(match, 'y'), 2);
    shouldBe(indexOfValue(match, undefined), 1);
}
