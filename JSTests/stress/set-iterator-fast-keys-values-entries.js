// Tests for fast iteration of JSSetIterator (set.values(), set.keys(), set.entries()).
// Note: Set.prototype.keys === Set.prototype.values; both produce JSSetIterator(Values).

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

function keys(set) {
    var sum = 0;
    for (var k of set.keys())
        sum += k;
    return sum;
}
noInline(keys);

function entries(set) {
    var sum = 0;
    for (var [a, b] of set.entries())
        sum += a + b;
    return sum;
}
noInline(entries);

// for (...of set) uses Set.prototype[Symbol.iterator] === Set.prototype.values (FastSet path,
// fresh JSSetIterator(Values)).
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

var expectedValues = 55;       // 1+2+...+10
var expectedEntries = 110;     // 2*(1+2+...+10) — entries yields [v, v]

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(set), expectedValues);
    shouldBe(keys(set), expectedValues);
    shouldBe(entries(set), expectedEntries);
    shouldBe(plain(set), expectedValues);
}

// Iterating the iterator object directly (the iterable is already a JSSetIterator).
function valuesOfIterator(set) {
    var iter = set.values();
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(valuesOfIterator);

function entriesOfIterator(set) {
    var iter = set.entries();
    var sum = 0;
    for (var [a, b] of iter)
        sum += a + b;
    return sum;
}
noInline(entriesOfIterator);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(valuesOfIterator(set), expectedValues);
    shouldBe(entriesOfIterator(set), expectedEntries);
}
