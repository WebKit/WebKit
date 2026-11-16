function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(`${message}: expected ${expected} but got ${actual}`);
}

function getObject(map, key) { return map.get(key); }
function getUntyped(map, key) { return map.get(key); }
function hasString(set, key) { return set.has(key); }
noInline(getObject);
noInline(getUntyped);
noInline(hasString);

let objects = [];
for (let i = 0; i < 64; ++i)
    objects.push({ i });

for (let i = 0; i < testLoopCount; ++i) {
    let map = new Map();
    let set = new Set();
    for (let j = 0; j < 16; ++j) {
        map.set(objects[j], j);
        map.set(j, j);
        map.set("s" + j, j);
        set.add("s" + j);
    }
    for (let j = 0; j < 16; j += 2) {
        map.delete(objects[j]);
        map.delete(j);
        map.delete("s" + j);
        set.delete("s" + j);
    }
    let j = i & 15;
    let expected = (j & 1) ? j : undefined;
    shouldBe(getObject(map, objects[j]), expected, "object key");
    shouldBe(getUntyped(map, j), expected, "int key");
    shouldBe(getUntyped(map, "s" + j), expected, "string key");
    shouldBe(getUntyped(map, 1.5), undefined, "missing double key");
    shouldBe(hasString(set, "s" + j), !!(j & 1), "set string key");
}

function sumSet(set) {
    let sum = 0;
    for (let value of set)
        sum += value;
    return sum;
}
noInline(sumSet);

for (let i = 0; i < testLoopCount; ++i) {
    let set = new Set([1, 2, 3, 4]);
    if (i & 1)
        set.delete(2);
    shouldBe(sumSet(set), (i & 1) ? 8 : 10, "Set iteration");
    shouldBe(sumSet(new Set()), 0, "empty Set iteration");
}
