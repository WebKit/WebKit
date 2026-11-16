// Mid-iteration mutation of the underlying Map. Per spec, entries added or removed during
// iteration must be visible / skipped consistently — JSC's transitAndNext handles this.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function consumeOneAndIterateRest(map) {
    var iter = map.values();
    var first = iter.next();
    var rest = [];
    for (var v of iter) rest.push(v);
    return { first: first.value, rest };
}
noInline(consumeOneAndIterateRest);

function consumeKeysRest(map) {
    var iter = map.keys();
    iter.next();
    var rest = [];
    for (var k of iter) rest.push(k);
    return rest;
}
noInline(consumeKeysRest);

function consumeEntriesRest(map) {
    var iter = map.entries();
    iter.next();
    var rest = [];
    for (var e of iter) rest.push(e[0] + ':' + e[1]);
    return rest;
}
noInline(consumeEntriesRest);

for (var i = 0; i < testLoopCount; ++i) {
    var map = new Map();
    for (var j = 0; j < 5; ++j) map.set(j, j * 10);

    var r = consumeOneAndIterateRest(map);
    shouldBe(r.first, 0);
    shouldBe(r.rest.join(','), '10,20,30,40');

    shouldBe(consumeKeysRest(map).join(','), '1,2,3,4');
    shouldBe(consumeEntriesRest(map).join(','), '1:10,2:20,3:30,4:40');
}

// Mutate during iteration: add an entry while iterating; the new entry should be visible.
function drainThenIterate() {
    var map = new Map();
    for (var i = 0; i < 3; ++i) map.set(i, i + 100);
    var collected = [];
    for (var [k, v] of map) {
        collected.push(k + ':' + v);
        if (k === 1) map.set(99, 999);
        if (k === 99) map.set(100, 1000);
    }
    return collected.join(',');
}
noInline(drainThenIterate);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(drainThenIterate(), '0:100,1:101,2:102,99:999,100:1000');
