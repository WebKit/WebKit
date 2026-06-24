// Tests for fast iteration of JSMapIterator (map.keys(), map.values(), map.entries()).

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

function keys(map) {
    var sum = 0;
    for (var k of map.keys())
        sum += k;
    return sum;
}
noInline(keys);

function entries(map) {
    var sum = 0;
    for (var [k, v] of map.entries())
        sum += k * 1000 + v;
    return sum;
}
noInline(entries);

// for (...of map) uses Map.prototype[Symbol.iterator] === Map.prototype.entries (FastMap path,
// fresh JSMapIterator(Entries)).
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

var expectedValues = 55;       // 1+2+...+10
var expectedKeys = 45;         // 0+1+...+9
var expectedEntries = 0;       // sum k*1000 + v for k in [0,9], v=k+1
for (var i = 0; i < 10; ++i)
    expectedEntries += i * 1000 + (i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(map), expectedValues);
    shouldBe(keys(map), expectedKeys);
    shouldBe(entries(map), expectedEntries);
    shouldBe(plain(map), expectedEntries);
}

// Iterating the iterator object directly (the iterable is already a JSMapIterator).
function valuesOfIterator(map) {
    var iter = map.values();
    var sum = 0;
    for (var v of iter)
        sum += v;
    return sum;
}
noInline(valuesOfIterator);

function keysOfIterator(map) {
    var iter = map.keys();
    var sum = 0;
    for (var k of iter)
        sum += k;
    return sum;
}
noInline(keysOfIterator);

function entriesOfIterator(map) {
    var iter = map.entries();
    var sum = 0;
    for (var [k, v] of iter)
        sum += k * 1000 + v;
    return sum;
}
noInline(entriesOfIterator);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(valuesOfIterator(map), expectedValues);
    shouldBe(keysOfIterator(map), expectedKeys);
    shouldBe(entriesOfIterator(map), expectedEntries);
}
