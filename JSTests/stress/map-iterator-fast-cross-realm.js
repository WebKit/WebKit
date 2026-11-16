// Cross-realm: iterating a Map / MapIterator from another realm.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

var otherRealm = createGlobalObject();

function valuesOfMap(map) {
    var sum = 0;
    for (var v of map.values()) sum += v;
    return sum;
}
noInline(valuesOfMap);

function keysOfMap(map) {
    var sum = 0;
    for (var k of map.keys()) sum += k;
    return sum;
}
noInline(keysOfMap);

function entriesOfMap(map) {
    var sum = 0;
    for (var [k, v] of map.entries()) sum += k * 1000 + v;
    return sum;
}
noInline(entriesOfMap);

function plainOfMap(map) {
    var sum = 0;
    for (var [k, v] of map) sum += k * 1000 + v;
    return sum;
}
noInline(plainOfMap);

var otherMakeMap = otherRealm.Function('m = new m.Map();' +
    'for (var i = 0; i < 10; ++i) m.set(i, i + 1);' +
    'return m;'.replace('m =', 'var m =').replace(/m\./g, 'this.'));
// Simpler: create the cross-realm map manually.
var otherMap = otherRealm.eval('var m = new Map(); for (var i = 0; i < 10; ++i) m.set(i, i + 1); m');

var expectedValues = 55;
var expectedKeys = 45;
var expectedEntries = 0;
for (var i = 0; i < 10; ++i) expectedEntries += i * 1000 + (i + 1);

// Cross-realm fast path — when we iterate a foreign-realm Map directly, getIterationMode bails to
// Generic because the structure check against this realm's mapIteratorStructure fails. The for-of
// still works, just via the slow path. Verify correctness.
for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(valuesOfMap(otherMap), expectedValues);
    shouldBe(keysOfMap(otherMap), expectedKeys);
    shouldBe(entriesOfMap(otherMap), expectedEntries);
    shouldBe(plainOfMap(otherMap), expectedEntries);
}
