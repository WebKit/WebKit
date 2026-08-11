function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected ' + expected);
}

function countValues(iterable) {
    var count = 0;
    for (var value of iterable)
        count++;
    return count;
}
noInline(countValues);

function sumValues(iterable) {
    var result = 0;
    for (var value of iterable)
        result += typeof value === 'string' ? value.length : 1;
    return result;
}
noInline(sumValues);

function* generatorValues() {
    yield 1;
    yield 2;
}

var array = [1, 2, 3];
var map = new Map([[1, 2], [3, 4]]);
var set = new Set([1, 2, 3, 4]);
var ascii = "abcde";
var unicode = "a\u{20BB7}b";

// Mixing more iterable types at one for-of site than the fast iteration mode limit must keep
// working correctly (the types over the limit fall back to generic iteration).
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(countValues(array), 3);
    shouldBe(countValues(map), 2);
    shouldBe(countValues(set), 4);
    shouldBe(countValues(ascii), 5);
    shouldBe(countValues(unicode), 3);
    shouldBe(countValues(generatorValues()), 2);
}

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumValues(ascii), 5);
    shouldBe(sumValues(unicode), 4);
    shouldBe(sumValues(set), 4);
    shouldBe(sumValues(array), 3);
    shouldBe(sumValues(map), 2);
}
