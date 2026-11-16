function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function splice0(array, start, deleteCount) {
    array.splice(start, deleteCount);
    return array;
}
noInline(splice0);

function splice1(array, start, deleteCount, a) {
    array.splice(start, deleteCount, a);
    return array;
}
noInline(splice1);

function splice2(array, start, deleteCount, a, b) {
    array.splice(start, deleteCount, a, b);
    return array;
}
noInline(splice2);

Array.prototype[2] = 'proto2';
Array.prototype[4] = 'proto4';

for (var i = 0; i < testLoopCount; ++i) {
    var array = splice1([1, 2, , 4], 0, 2, 9);
    shouldBeArray(array, [9, 'proto2', 4]);
    shouldBe(array.hasOwnProperty(1), true);

    shouldBeArray(splice2([1, 2, 3, , 5, 6], 0, 3, 7, 8), [7, 8, 'proto2', 5, 6]);
    shouldBeArray(splice1([1.5, 2.5, , 4.5], 0, 2, 9.5), [9.5, 'proto2', 4.5]);
    shouldBeArray(splice0([1, 2, , 4, 5], 0, 1), [2, 'proto2', 4, 5]);
    shouldBeArray(splice2([1, , 3, 4], 0, 3, 7, 8), [7, 8, 4]);
    shouldBeArray(splice1([1, 2, 3, 4, , 6, 7], 0, 2, 9), [9, 3, 4, 'proto4', 6, 7]);
    shouldBeArray(splice2([1, 2, 3, , 5], 1, 1, 7, 8), [1, 7, 8, 3, 'proto4', 5]);
}
