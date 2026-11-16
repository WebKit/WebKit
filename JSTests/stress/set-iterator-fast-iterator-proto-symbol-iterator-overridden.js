// Replacing %IteratorPrototype%[Symbol.iterator] (which the JSSetIterator inherits) should
// invalidate the fast path for set iterator reuse via for-of.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function valuesOfIterator(set) {
    var iter = set.values();
    var sum = 0;
    for (var v of iter) sum += v;
    return sum;
}
noInline(valuesOfIterator);

var set = new Set();
for (var i = 0; i < 10; ++i)
    set.add(i + 1);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(valuesOfIterator(set), 55);

var iteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf((new Set()).values()));
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

shouldBe(valuesOfIterator(set), 550);
