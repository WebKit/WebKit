// Replacing Map.prototype[Symbol.iterator] fires mapIteratorProtocolWatchpointSet.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(map) {
    var sum = 0;
    for (var v of map.values())
        sum += v;
    return sum;
}
noInline(values);

function plain(map) {
    var sum = 0;
    for (var [k, v] of map)
        sum += k * 1000 + v;
    return sum;
}
noInline(plain);

var map = new Map();
for (var i = 0; i < 10; ++i)
    map.set(i, i + 1);

var expectedValues = 55;
var expectedPlain = 0;
for (var i = 0; i < 10; ++i)
    expectedPlain += i * 1000 + (i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(map), expectedValues);
    shouldBe(plain(map), expectedPlain);
}

// Replace Map.prototype[Symbol.iterator]; for-of map now goes through this.
Map.prototype[Symbol.iterator] = function () {
    var arr = [];
    for (var k of this.keys()) arr.push([k, k * 10]);
    var idx = 0;
    return {
        next() {
            return idx < arr.length ? { value: arr[idx++], done: false } : { value: undefined, done: true };
        }
    };
};

// `for (var [k, v] of map)` now: v=k*10, expected sum k*1000 + k*10 = k*1010.
var expectedAfter = 0;
for (var i = 0; i < 10; ++i)
    expectedAfter += i * 1010;
shouldBe(plain(map), expectedAfter);

// `map.values()` is unaffected — we replaced [Symbol.iterator], not .values.
shouldBe(values(map), expectedValues);
