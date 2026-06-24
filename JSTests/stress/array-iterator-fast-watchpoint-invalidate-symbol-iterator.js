// Replacing Array.prototype[Symbol.iterator] fires arrayIteratorProtocolWatchpointSet.
// Functions warmed up under the fast path must keep producing correct results after the
// watchpoint fires (they should bail to the generic path or OSR-exit).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(array) {
    var sum = 0;
    for (var v of array.values())
        sum += v;
    return sum;
}
noInline(values);

function plain(array) {
    var sum = 0;
    for (var v of array)
        sum += v;
    return sum;
}
noInline(plain);

var array = [];
for (var i = 0; i < 10; ++i)
    array.push(i + 1);

// Warm up under the fast path.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(array), 55);
    shouldBe(plain(array), 55);
}

// Now fire the watchpoint by replacing Array.prototype[Symbol.iterator].
Array.prototype[Symbol.iterator] = function () {
    var idx = 0;
    var arr = this;
    return {
        next() {
            return idx < arr.length ? { value: arr[idx++] * 10, done: false } : { value: undefined, done: true };
        }
    };
};

// `for (var v of array)` now uses the replaced iterator: 10+20+...+100 = 550.
shouldBe(plain(array), 550);

// `array.values()` is unaffected — we replaced [Symbol.iterator], not .values.
shouldBe(values(array), 55);
