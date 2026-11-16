// Replacing MapIterator.prototype.next fires mapIteratorProtocolWatchpointSet.
// Functions warmed up under the fast path must keep producing correct results.

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

// Replace MapIterator.prototype.next — fires the watchpoint.
var mapIteratorPrototype = Object.getPrototypeOf((new Map()).values());
var counter = 0;
mapIteratorPrototype.next = function () {
    counter++;
    return counter <= 3 ? { value: 7, done: false } : { value: undefined, done: true };
};

counter = 0;
shouldBe(values(map), 21); // 7 + 7 + 7
// `for (var [k, v] of map)` after the override iterates over scalar 7s; destructuring 7 as [k, v]
// invokes 7[Symbol.iterator]() which throws TypeError.
counter = 0;
var threw = false;
try { plain(map); } catch (e) { threw = e instanceof TypeError; }
shouldBe(threw, true);
