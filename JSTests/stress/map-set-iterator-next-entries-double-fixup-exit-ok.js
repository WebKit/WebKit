// Regression test for the DFG Fixup ValueRep placement in the inlined
// Map/Set iterator next() entries path. That path emits MapIteratorKey /
// MapIteratorValue and then NewArray for the [key, value] result. With
// Double-typed keys/values, Fixup inserts representation conversions on the
// NewArray operands, so the entries block must keep a valid exit origin
// (emitExitOK in effect) when NewArray is created, mirroring
// array-iterator-fast-entries-double-array-fixup-exit-ok.js.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

// Map.entries() with Double keys and Double values, advanced via direct next().
function sumMapEntries(map) {
    var result = 0;
    var iterator = map.entries();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value[0] + step.value[1];
    return result;
}
noInline(sumMapEntries);

// Set.entries() with Double keys, advanced via direct next().
function sumSetEntries(set) {
    var result = 0;
    var iterator = set.entries();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value[0] + step.value[1];
    return result;
}
noInline(sumSetEntries);

var map = new Map();
var set = new Set();
var expectedMap = 0;
var expectedSet = 0;
for (var i = 0; i < 5; ++i) {
    var key = 268435456.5 + i;
    var value = 4294967295.25 + i;
    map.set(key, value);
    set.add(key);
    expectedMap += key + value;
    expectedSet += key + key;
}

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(sumMapEntries(map), expectedMap);
    shouldBe(sumSetEntries(set), expectedSet);
}
