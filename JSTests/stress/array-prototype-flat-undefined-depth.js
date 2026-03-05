// This test verifies that Array.prototype.flat handles undefined depth correctly.
// Per ECMAScript spec, when depth is undefined, the default depth of 1 should be used.
// This was a bug in the C++ optimization where flat(undefined) was treated as flat(0)
// instead of flat(1), which broke Google Docs.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (var i = 0; i < expected.length; ++i) {
        if (Array.isArray(expected[i])) {
            shouldBe(Array.isArray(actual[i]), true);
            shouldBeArray(actual[i], expected[i]);
        } else {
            shouldBe(actual[i], expected[i]);
        }
    }
}

// Test: flat() with no arguments should use depth 1
shouldBeArray([[1, 2], [3, 4]].flat(), [1, 2, 3, 4]);
shouldBeArray([[[1]]].flat(), [[1]]);

// Test: flat(undefined) should behave the same as flat() - using default depth 1
// This was the bug: flat(undefined) was treating undefined as 0 instead of defaulting to 1
shouldBeArray([[1, 2], [3, 4]].flat(undefined), [1, 2, 3, 4]);
shouldBeArray([[[1]]].flat(undefined), [[1]]);

// Verify flat() and flat(undefined) produce identical results
var testArrays = [
    [[1, 2], [3, 4]],
    [[[1]]],
    [[1], [[2]], [[[3]]]],
    [[], [1], [], [2, 3]],
    [[1, [2, [3]]]],
];

for (var arr of testArrays) {
    var resultNoArg = arr.flat();
    var resultUndefined = arr.flat(undefined);
    shouldBeArray(resultNoArg, resultUndefined);
}

// Test: flat(0) should NOT flatten (different from undefined)
shouldBeArray([[1, 2], [3, 4]].flat(0), [[1, 2], [3, 4]]);
shouldBeArray([[[1]]].flat(0), [[[1]]]);

// Test: flat(null) should convert to 0 via ToIntegerOrInfinity
shouldBeArray([[1, 2], [3, 4]].flat(null), [[1, 2], [3, 4]]);

// Test: flat(NaN) should convert to 0 via ToIntegerOrInfinity
shouldBeArray([[1, 2], [3, 4]].flat(NaN), [[1, 2], [3, 4]]);

// Test: explicit flat(1) should match flat() and flat(undefined)
for (var arr of testArrays) {
    var resultNoArg = arr.flat();
    var resultOne = arr.flat(1);
    shouldBeArray(resultNoArg, resultOne);
}

// Stress test: run many iterations to catch JIT issues
for (var i = 0; i < 1000; i++) {
    var arr = [[i, i + 1], [i + 2]];
    var expected = [i, i + 1, i + 2];

    // All three should produce the same result
    shouldBeArray(arr.flat(), expected);
    shouldBeArray(arr.flat(undefined), expected);
    shouldBeArray(arr.flat(1), expected);
}

// Test with various array types (contiguous int, double, object)
shouldBeArray([[1, 2], [3, 4]].flat(undefined), [1, 2, 3, 4]); // int32
shouldBeArray([[1.5, 2.5], [3.5]].flat(undefined), [1.5, 2.5, 3.5]); // double

// Test with objects (compare by reference)
var obj1 = {a: 1};
var obj2 = {b: 2};
var objectResult = [[obj1], [obj2]].flat(undefined);
shouldBe(objectResult.length, 2);
shouldBe(objectResult[0], obj1);
shouldBe(objectResult[1], obj2);

// Edge case: deeply nested with undefined should flatten one level
var deeplyNested = [[[[[[1]]]]]];
shouldBeArray(deeplyNested.flat(undefined), [[[[[1]]]]]);
