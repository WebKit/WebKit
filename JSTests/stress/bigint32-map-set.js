function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testMap(map, key)
{
    return map.has(key);
}
noInline(testMap);

function testSet(set, key)
{
    return set.has(key);
}
noInline(testSet);

let map = new Map();
map.set("Hey", "Hey");
map.set(null, null);
map.set(1n, 1n);
map.set(2n, 2n);
map.set(0xffffffffffffffffn, 0xffffffffffffffffn);
map.set("Hello", "Hello");

let set = new Set();
set.add("Hey");
set.add(null);
set.add(1n);
set.add(2n);
set.add(0xffffffffffffffffn);
set.add("Hello");

for (let i = 0; i < 1e4; ++i) {
    shouldBe(testMap(map, 1n), true);
    shouldBe(testSet(set, 1n), true);
    shouldBe(testMap(map, 2n), true);
    shouldBe(testSet(set, 2n), true);
    shouldBe(testMap(map, 3n), false);
    shouldBe(testSet(set, 3n), false);
}
