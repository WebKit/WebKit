// Replacing %IteratorPrototype%[Symbol.iterator] is NOT covered by arrayIteratorProtocolWatchpointSet,
// but the dynamic CheckIsConstant on the symbolIterator value should catch it and bail to generic.
// arrayIterator[Symbol.iterator] is inherited from %IteratorPrototype% — replace it and verify
// the fast path bails out.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function valuesOfIterator(array) {
    var iter = array.values();
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(valuesOfIterator);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

// Warm up under FastArrayValues path (iterating an existing JSArrayIterator).
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(array), 55);

// Replace %IteratorPrototype%[Symbol.iterator]. arrayIterator[Symbol.iterator] now resolves to this
// new function (CheckIsConstant on the symbolIterator value should bail).
var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([].values()));
iteratorPrototype[Symbol.iterator] = function () {
    // Return a fresh iterator over [99, 99].
    return [99, 99][Symbol.iterator]();
};

// `array.values()` returns a JSArrayIterator. `for (var v of iter)` calls iter[Symbol.iterator]()
// which now returns the [99, 99] iterator → 99 + 99 = 198.
shouldBe(valuesOfIterator(array), 198);
