// Replacing %IteratorPrototype%[Symbol.iterator] (which the JSMapIterator inherits) should
// invalidate the fast path for map iterator reuse via for-of.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function valuesOfIterator(map) {
    var iter = map.values();
    var sum = 0;
    for (var v of iter) sum += v;
    return sum;
}
noInline(valuesOfIterator);

var map = new Map();
for (var i = 0; i < 10; ++i)
    map.set(i, i + 1);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(map), 55);

// %IteratorPrototype% is the prototype of MapIteratorPrototype.
var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf((new Map()).values()));
iteratorPrototype[Symbol.iterator] = function () {
    var arr = [];
    var src = this;
    var nextResult;
    while (true) {
        nextResult = src.next();
        if (nextResult.done) break;
        arr.push(nextResult.value * 10);
    }
    var idx = 0;
    return {
        next() {
            return idx < arr.length ? { value: arr[idx++], done: false } : { value: undefined, done: true };
        }
    };
};

// `for (var v of iter)` now goes through the replaced [Symbol.iterator].
shouldBe(valuesOfIterator(map), 550);
