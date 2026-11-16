function assert(b) {
    if (!b)
        throw new Error("bad assertion");
}

function createWeakMap() {
    return new WeakMap();
}
noInline(createWeakMap);

function createWeakSet() {
    return new WeakSet();
}
noInline(createWeakSet);

var key = {};
for (var i = 0; i < 1e5; ++i) {
    var map = createWeakMap();
    assert(map instanceof WeakMap);
    assert(Object.getPrototypeOf(map) === WeakMap.prototype);
    assert(map.get(key) === undefined);
    assert(!map.has(key));
    map.set(key, i);
    assert(map.get(key) === i);
    assert(map.has(key));

    var set = createWeakSet();
    assert(set instanceof WeakSet);
    assert(Object.getPrototypeOf(set) === WeakSet.prototype);
    assert(!set.has(key));
    set.add(key);
    assert(set.has(key));
}

// Constructor with an iterable argument should keep working.
function createWeakMapWithEntries(k, v) {
    return new WeakMap([[k, v]]);
}
noInline(createWeakMapWithEntries);

function createWeakSetWithEntries(k) {
    return new WeakSet([k]);
}
noInline(createWeakSetWithEntries);

for (var i = 0; i < 1e4; ++i) {
    var k = {};
    var map = createWeakMapWithEntries(k, i);
    assert(map.get(k) === i);
    var set = createWeakSetWithEntries(k);
    assert(set.has(k));
}

// Subclassing (new.target !== WeakMap/WeakSet) should keep working.
class MyWeakMap extends WeakMap {}
class MyWeakSet extends WeakSet {}

function createSubclassed() {
    return [new MyWeakMap(), new MyWeakSet()];
}
noInline(createSubclassed);

for (var i = 0; i < 1e4; ++i) {
    var [map, set] = createSubclassed();
    assert(map instanceof MyWeakMap);
    assert(map instanceof WeakMap);
    assert(Object.getPrototypeOf(map) === MyWeakMap.prototype);
    map.set(key, i);
    assert(map.get(key) === i);

    assert(set instanceof MyWeakSet);
    assert(set instanceof WeakSet);
    assert(Object.getPrototypeOf(set) === MyWeakSet.prototype);
    set.add(key);
    assert(set.has(key));
}
