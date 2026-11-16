// Regression test for https://bugs.webkit.org/show_bug.cgi?id=315650
//
// MapIteratorNext is modeled statelessly: it returns the advanced (storage, entry)
// pair, and the parser commits it back into the iterator via PutInternalField nodes
// emitted *after* the key/value loads. This gives two properties we verify here:
//
//   1. ObjectAllocationSinking can eliminate the JSMapIterator/JSSetIterator in for-of.
//      Iteration must still produce the correct sequence even when the iterator object
//      is never allocated.
//   2. The irreversible advance is published only once everything is done, so an OSR
//      exit (or escape) part-way through must leave the iterator in a consistent state:
//      never partially advanced, never double-advanced.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

// 1. for-of over Map/Set with a sunk iterator, across keys/values/entries.
(function () {
    var map = new Map();
    var set = new Set();
    var keySum = 0, valSum = 0;
    for (var i = 0; i < 64; ++i) {
        map.set(i, i * 2);
        set.add(i);
        keySum += i;
        valSum += i * 2;
    }

    function sumMapKeys(map) {
        var r = 0;
        for (var k of map.keys())
            r += k;
        return r;
    }
    function sumMapValues(map) {
        var r = 0;
        for (var v of map.values())
            r += v;
        return r;
    }
    function sumMapEntries(map) {
        var r = 0;
        for (var [k, v] of map) {
            if (v !== k * 2)
                throw new Error('entry mismatch: ' + k + ' -> ' + v);
            r += k;
        }
        return r;
    }
    function sumSet(set) {
        var r = 0;
        for (var k of set)
            r += k;
        return r;
    }
    function sumSetEntries(set) {
        var r = 0;
        for (var [a, b] of set.entries()) {
            if (a !== b)
                throw new Error('set entry mismatch: ' + a + ' vs ' + b);
            r += a;
        }
        return r;
    }
    noInline(sumMapKeys);
    noInline(sumMapValues);
    noInline(sumMapEntries);
    noInline(sumSet);
    noInline(sumSetEntries);

    for (var i = 0; i < testLoopCount; ++i) {
        shouldBe(sumMapKeys(map), keySum);
        shouldBe(sumMapValues(map), valSum);
        shouldBe(sumMapEntries(map), keySum);
        shouldBe(sumSet(set), keySum);
        shouldBe(sumSetEntries(set), keySum);
    }
})();

// 2. The iterator escapes on a rare path after a partial advance. Because the advance is
//    committed before the iterator can be observed, the materialized iterator must resume
//    from exactly the next live entry.
(function () {
    var map = new Map([[10, 100], [20, 200], [30, 300], [40, 400]]);
    var mapIteratorPrototype = map.values().__proto__;

    function test(map, flag) {
        var iterator = map.values();
        var first = iterator.next().value;   // advance once: now positioned after entry 0
        var second = iterator.next().value;  // advance twice: now positioned after entry 1
        if (flag)
            return iterator;                 // escape -> force materialization
        return first + second;
    }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i) {
        var flag = (i % 1234) === 0;
        var result = test(map, flag);
        if (flag) {
            shouldBe(typeof result, "object");
            shouldBe(result.__proto__, mapIteratorPrototype);
            // The escaped iterator was advanced exactly twice, so the next two values
            // must be the 3rd and 4th, then done.
            shouldBe(result.next().value, 300);
            shouldBe(result.next().value, 400);
            shouldBe(result.next().done, true);
        } else
            shouldBe(result, 100 + 200);
    }
})();

// 2b. Same escape-after-partial-advance property for Set.
(function () {
    var set = new Set([1, 2, 3, 4, 5]);
    var setIteratorPrototype = set.values().__proto__;

    function test(set, flag) {
        var iterator = set.values();
        iterator.next();
        iterator.next();
        iterator.next();
        if (flag)
            return iterator;
        return true;
    }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i) {
        var flag = (i % 1234) === 0;
        var result = test(set, flag);
        if (flag) {
            shouldBe(result.__proto__, setIteratorPrototype);
            shouldBe(result.next().value, 4);
            shouldBe(result.next().value, 5);
            shouldBe(result.next().done, true);
        } else
            shouldBe(result, true);
    }
})();

// 3. Force an OSR exit immediately after an advance. The iterator must not be left
//    partially advanced: re-running from the checkpoint after the exit must continue
//    the iteration cleanly without skipping or repeating an element.
(function () {
    var map = new Map([[0, 0], [1, 1], [2, 2]]);
    var mapIteratorPrototype = map.values().__proto__;

    function test(map, flag) {
        var iterator = map.values();
        iterator.next();
        if (flag) {
            OSRExit();
            return iterator;
        }
        return iterator.next().value;
    }
    noInline(test);

    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(test(map, false), 1);

    var iterator = test(map, true);
    shouldBe(iterator.__proto__, mapIteratorPrototype);
    shouldBe(iterator.next().value, 1);
    shouldBe(iterator.next().value, 2);
    shouldBe(iterator.next().done, true);
})();

// 4. Mutation during iteration drives the stateless slow path (obsolete/rehashed tables).
//    Entries added during iteration are visited; entries deleted before being visited are
//    skipped. This must hold once the loop tiers up to DFG/FTL.
(function () {
    function run() {
        var m = new Map();
        for (var i = 0; i < 8; ++i)
            m.set(i, i);
        var sum = 0, c = 0;
        for (var [k, v] of m) {
            if (v !== k)
                throw new Error('value mismatch: ' + k + ' -> ' + v);
            sum += k;
            if (c === 2) {
                for (var j = 100; j < 140; ++j) // force rehash + obsolete-table chain
                    m.set(j, j);
            }
            if (c === 4) {
                m.delete(5);
                m.delete(6);
                m.delete(7);
            }
            ++c;
        }
        return sum;
    }
    noInline(run);

    var expected = (0 + 1 + 2 + 3 + 4); // 5, 6, 7 deleted before being visited
    for (var j = 100; j < 140; ++j)
        expected += j;
    for (var i = 0; i < testLoopCount; ++i)
        shouldBe(run(), expected);
})();
