// Exercises the lazily-allocated WeakMap/WeakSet buffer: a freshly constructed instance
// shares an immortal empty buffer (capacity 1) and only allocates a real buffer on the
// first insertion. All read paths must work against that shared empty buffer, and the
// getOrInsert fast path (findBucketIndex + addBucket) must not write into it.

function assert(b, message) {
    if (!b)
        throw new Error("bad assertion: " + message);
}

function makeWeakMap() {
    return new WeakMap();
}
noInline(makeWeakMap);

function makeWeakSet() {
    return new WeakSet();
}
noInline(makeWeakSet);

// 1. Reads against the shared empty buffer must miss, and delete must report false.
for (var i = 0; i < 1e5; ++i) {
    var key = {};
    var map = makeWeakMap();
    assert(map.get(key) === undefined, "empty map get");
    assert(!map.has(key), "empty map has");
    assert(!map.delete(key), "empty map delete");

    var set = makeWeakSet();
    assert(!set.has(key), "empty set has");
    assert(!set.delete(key), "empty set delete");
}

// 2. getOrInsert / getOrInsertComputed on a fresh (empty buffer) map must allocate a real
//    buffer and insert, rather than writing into the shared sentinel at the stale index.
for (var i = 0; i < 1e5; ++i) {
    var key = {};
    var map = makeWeakMap();
    var inserted = map.getOrInsert(key, i);
    assert(inserted === i, "getOrInsert returns inserted value");
    assert(map.get(key) === i, "getOrInsert stored value");
    assert(map.has(key), "getOrInsert key present");
    // Second call must observe the existing entry (real-buffer exists path), not overwrite.
    assert(map.getOrInsert(key, i + 1) === i, "getOrInsert keeps existing value");

    var key2 = {};
    var map2 = makeWeakMap();
    var computed = map2.getOrInsertComputed(key2, (k) => i * 2);
    assert(computed === i * 2, "getOrInsertComputed returns value");
    assert(map2.get(key2) === i * 2, "getOrInsertComputed stored value");
}

// 3. A separate empty map sharing the same sentinel must remain empty after another map
//    allocated its own real buffer (i.e. addBucket did not scribble on the shared buffer).
var canary = {};
for (var i = 0; i < 1e4; ++i) {
    var emptyMap = makeWeakMap();
    var usedMap = makeWeakMap();
    usedMap.set(canary, 123);
    assert(!emptyMap.has(canary), "sibling empty map stays empty after another map writes");
    assert(emptyMap.get(canary) === undefined, "sibling empty map get misses");

    var emptySet = makeWeakSet();
    var usedSet = makeWeakSet();
    usedSet.add(canary);
    assert(!emptySet.has(canary), "sibling empty set stays empty");
}

// 4. Grow a map enough to force several rehashes (real-buffer reallocation + free), then
//    verify every entry survives.
function makeFilledMap(n) {
    var map = new WeakMap();
    var keys = [];
    for (var j = 0; j < n; ++j) {
        var k = { id: j };
        keys.push(k);
        map.set(k, j);
    }
    return [map, keys];
}
noInline(makeFilledMap);

for (var round = 0; round < 100; ++round) {
    var [map, keys] = makeFilledMap(256);
    for (var j = 0; j < keys.length; ++j)
        assert(map.get(keys[j]) === j, "filled map entry " + j);
    // Delete half to exercise shrink/rehash.
    for (var j = 0; j < keys.length; j += 2)
        assert(map.delete(keys[j]), "delete entry " + j);
    for (var j = 1; j < keys.length; j += 2)
        assert(map.get(keys[j]) === j, "surviving entry " + j);
}

// 5. Allocate many empty (sentinel) maps and force GC. finalizeUnconditionally must not
//    touch / free the shared empty buffer for these instances.
var live = [];
for (var i = 0; i < 1e4; ++i)
    live.push(makeWeakMap(), makeWeakSet());
if (typeof gc === "function")
    gc();
for (var i = 0; i < live.length; ++i) {
    var obj = live[i];
    assert(!obj.has(canary), "post-gc empty container has");
}

// 6. Subclasses (slow allocation path / new.target != callee) must also start empty.
class SubWeakMap extends WeakMap {}
function makeSub() {
    return new SubWeakMap();
}
noInline(makeSub);
for (var i = 0; i < 1e4; ++i) {
    var m = makeSub();
    assert(m.get(canary) === undefined, "subclass empty get");
    m.set(canary, i);
    assert(m.get(canary) === i, "subclass set/get");
}

// 7. Hand a fresh (still empty-buffer) map/set to read functions that were optimized against a
//    populated one, so the DFG/FTL WeakMapGet fast path probes the shared empty buffer and misses.
function readGet(map, key) {
    return map.get(key);
}
noInline(readGet);

function readHas(map, key) {
    return map.has(key);
}
noInline(readHas);

function readSetHas(set, key) {
    return set.has(key);
}
noInline(readSetHas);

var warmKey = {};
var warmMap = new WeakMap();
warmMap.set(warmKey, 1);
var warmSet = new WeakSet();
warmSet.add(warmKey);
for (var i = 0; i < 1e5; ++i) {
    assert(readGet(warmMap, warmKey) === 1, "warm get");
    assert(readHas(warmMap, warmKey), "warm has");
    assert(readSetHas(warmSet, warmKey), "warm set has");
}
for (var i = 0; i < 1e4; ++i) {
    var emptyMap = makeWeakMap();
    var emptySet = makeWeakSet();
    assert(readGet(emptyMap, warmKey) === undefined, "optimized get on empty map");
    assert(!readHas(emptyMap, warmKey), "optimized has on empty map");
    assert(!readSetHas(emptySet, warmKey), "optimized has on empty set");
}
