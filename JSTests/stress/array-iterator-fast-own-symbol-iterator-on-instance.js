// Adding own [Symbol.iterator] to a JSArrayIterator changes its structure → CheckStructure should
// fail and the fast path bails to generic.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function sumIterator(iter) {
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(sumIterator);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

// Warm up.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(sumIterator(array.values()), 55);

// Now add own [Symbol.iterator] — structure differs from default arrayIteratorStructure.
var customIter = array.values();
customIter[Symbol.iterator] = function () { return [777][Symbol.iterator](); };

shouldBe(sumIterator(customIter), 777);

// Also run a few more times after warmup to confirm we don't crash from a stale structure speculation.
for (var i = 0; i < testLoopCount; ++i)
    shouldBe(sumIterator(array.values()), 55);
