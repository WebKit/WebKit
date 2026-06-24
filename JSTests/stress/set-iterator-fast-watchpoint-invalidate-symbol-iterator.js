// Replacing Set.prototype[Symbol.iterator] fires setIteratorProtocolWatchpointSet.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(set) {
    var sum = 0;
    for (var v of set.values())
        sum += v;
    return sum;
}
noInline(values);

function plain(set) {
    var sum = 0;
    for (var v of set)
        sum += v;
    return sum;
}
noInline(plain);

var set = new Set();
for (var i = 0; i < 10; ++i)
    set.add(i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(set), 55);
    shouldBe(plain(set), 55);
}

// Replace Set.prototype[Symbol.iterator]; for-of set now goes through this.
Set.prototype[Symbol.iterator] = function () {
    var arr = [];
    for (var v of this.values()) arr.push(v * 10);
    var idx = 0;
    return {
        next() {
            return idx < arr.length ? { value: arr[idx++], done: false } : { value: undefined, done: true };
        }
    };
};

// `for (var v of set)` now: v*10 each, expected sum = 550.
shouldBe(plain(set), 550);

// `set.values()` is unaffected.
shouldBe(values(set), 55);
