function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function createSet(iterable) {
    return new Set(iterable);
}

function createMap(iterable) {
    return new Map(iterable);
}

function createWeakMap(iterable) {
    return new WeakMap(iterable);
}

function createWeakSet(iterable) {
    return new WeakSet(iterable);
}

function test() {
    var set = createSet();
    var setFromNull = createSet(null);
    var setFromArray = createSet([1, 2, 2]);
    var map = createMap(undefined);
    var mapFromArray = createMap([[1, 2]]);
    var weakMap = createWeakMap(null);
    var weakSet = createWeakSet();
    return [set, setFromNull, setFromArray, map, mapFromArray, weakMap, weakSet];
}
noInline(test);

var key = { };
for (var i = 0; i < testLoopCount; ++i) {
    var [set, setFromNull, setFromArray, map, mapFromArray, weakMap, weakSet] = test();
    shouldBe(set instanceof Set, true);
    shouldBe(set.size, 0);
    shouldBe(setFromNull.size, 0);
    shouldBe(setFromArray.size, 2);
    shouldBe(map instanceof Map, true);
    shouldBe(map.size, 0);
    shouldBe(mapFromArray.get(1), 2);
    shouldBe(weakMap instanceof WeakMap, true);
    shouldBe(weakMap.has(key), false);
    shouldBe(weakSet instanceof WeakSet, true);
    set.add(i);
    shouldBe(set.has(i), true);
}
