function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function opaque1n()
{
    return 1n;
}
noInline(opaque1n);

function testMap(map) {
    map.set(1n, 42);
    shouldBe(map.has(1n), true);
    shouldBe(map.has(opaque1n()), true);
    shouldBe(map.has(createHeapBigInt(opaque1n())), true);
    shouldBe(map.get(1n), 42);
    shouldBe(map.get(opaque1n()), 42);
    shouldBe(map.get(createHeapBigInt(opaque1n())), 42);
    map.set(1n, 40);
    shouldBe(map.get(1n), 40);
    shouldBe(map.get(opaque1n()), 40);
    shouldBe(map.get(createHeapBigInt(opaque1n())), 40);
}
noInline(testMap);

function testSet(set) {
    set.add(1n);
    shouldBe(set.has(1n), true);
    shouldBe(set.has(opaque1n()), true);
    shouldBe(set.has(createHeapBigInt(opaque1n())), true);
    set.delete(createHeapBigInt(opaque1n()));
    shouldBe(set.has(1n), false);
    shouldBe(set.has(opaque1n()), false);
    shouldBe(set.has(createHeapBigInt(opaque1n())), false);
}
noInline(testSet);

let map = new Map();
let set = new Set();
for (let i = 0; i < 1e4; ++i) {
    testMap(map);
    testSet(set);
}
