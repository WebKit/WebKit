// Mid-iteration mutation of the underlying Set.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function consumeOneAndIterateRest(set) {
    var iter = set.values();
    var first = iter.next();
    var rest = [];
    for (var v of iter) rest.push(v);
    return { first: first.value, rest };
}
noInline(consumeOneAndIterateRest);

for (var i = 0; i < testLoopCount; ++i) {
    var set = new Set();
    for (var j = 0; j < 5; ++j) set.add(j * 10);

    var r = consumeOneAndIterateRest(set);
    shouldBe(r.first, 0);
    shouldBe(r.rest.join(','), '10,20,30,40');
}

// Add during iteration; new entries are visible.
function drainThenIterate() {
    var set = new Set([1, 2, 3]);
    var collected = [];
    for (var v of set) {
        collected.push(v);
        if (v === 2) set.add(99);
        if (v === 99) set.add(100);
    }
    return collected.join(',');
}
noInline(drainThenIterate);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(drainThenIterate(), '1,2,3,99,100');

// Delete during iteration; deleted entries that haven't been visited yet are skipped.
function deleteWhileIterating() {
    var set = new Set([1, 2, 3, 4]);
    var collected = [];
    for (var v of set) {
        collected.push(v);
        if (v === 2) set.delete(3);
    }
    return collected.join(',');
}
noInline(deleteWhileIterating);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(deleteWhileIterating(), '1,2,4');
