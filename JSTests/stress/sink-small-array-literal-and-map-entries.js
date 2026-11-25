function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function swap(a, b) {
    [a, b] = [b, a];
    return a.v - b.v;
}
noInline(swap);

function escapeSometimes(a, b, escape) {
    let array = [a, b, a];
    if (escape)
        return array;
    return array[0].v + array[2].v;
}
noInline(escapeSometimes);

function exitWithSunkArray(a, b, c) {
    let array = [a, b];
    let result = c + 1;
    return array[0].v + array[1].v + result;
}
noInline(exitWithSunkArray);

function sumMapEntries(map) {
    let result = 0;
    for (let [key, value] of map)
        result += key * value;
    return result;
}
noInline(sumMapEntries);

function findInMap(map, target) {
    for (let [key, value] of map) {
        if (value === target)
            return key;
    }
    return -1;
}
noInline(findInMap);

function sumArrayEntries(array) {
    let result = 0;
    for (let [index, value] of array.entries())
        result += index + value;
    return result;
}
noInline(sumArrayEntries);

function sumSetEntries(set) {
    let result = 0;
    for (let [a, b] of set.entries())
        result += a + b;
    return result;
}
noInline(sumSetEntries);

function deleteWhileIterating(map) {
    let count = 0;
    for (let [key] of map) {
        map.delete(key + 1);
        ++count;
    }
    return count;
}
noInline(deleteWhileIterating);

function* entriesOf(map) {
    for (let [key, value] of map)
        yield [value, key];
}

function sumGeneratorEntries(map) {
    let result = 0;
    for (let [value, key] of entriesOf(map))
        result += value - key;
    return result;
}
noInline(sumGeneratorEntries);

function keepEntries(map) {
    let kept = [];
    for (let entry of map)
        kept.push(entry);
    return kept;
}
noInline(keepEntries);

let map = new Map;
for (let i = 0; i < 10; ++i)
    map.set(i, i * 2);
let expectedMapSum = 0;
for (let i = 0; i < 10; ++i)
    expectedMapSum += i * i * 2;
let set = new Set([1, 2, 3]);
let mutableMap = new Map;
let fixedBox = { v: 3 };
let otherBox = { v: 2 };
let unitBox = { v: 1 };

for (let i = 0; i < testLoopCount; ++i) {
    let box = { v: i };
    shouldBe(swap(box, fixedBox), 3 - i);
    let escaped = escapeSometimes(box, otherBox, (i % 97) === 0);
    if ((i % 97) === 0) {
        shouldBe(escaped.length, 3);
        shouldBe(escaped[0].v, i);
        shouldBe(escaped[1].v, 2);
        shouldBe(escaped[2].v, i);
    } else
        shouldBe(escaped, i * 2);
    shouldBe(exitWithSunkArray(box, unitBox, i), i + 1 + i + 1);
    shouldBe(sumMapEntries(map), expectedMapSum);
    shouldBe(findInMap(map, 8), 4);
    shouldBe(findInMap(map, -1), -1);
    shouldBe(sumArrayEntries([10, 20, 30]), 63);
    shouldBe(sumSetEntries(set), 12);
    shouldBe(sumGeneratorEntries(map), 45);
    let kept = keepEntries(map);
    shouldBe(kept.length, 10);
    shouldBe(kept[3][0], 3);
    shouldBe(kept[3][1], 6);
    shouldBe(kept[3] !== kept[4], true);
    mutableMap.clear();
    mutableMap.set(0, 0);
    mutableMap.set(1, 1);
    mutableMap.set(2, 2);
    mutableMap.set(3, 3);
    shouldBe(deleteWhileIterating(mutableMap), 2);
}

shouldBe(exitWithSunkArray({ v: "a" }, { v: "b" }, 1.5), "ab2.5");
shouldBe(exitWithSunkArray({ v: 1 }, { v: 2 }, "x"), "3x1");

function sinkObjectIntoUnsaneArrayPrototype(cond, x) {
    let obj = { x };
    if (cond)
        return [obj];
    return obj.x;
}
noInline(sinkObjectIntoUnsaneArrayPrototype);

Array.prototype[0] = 42;
for (let i = 0; i < testLoopCount; ++i) {
    let r = sinkObjectIntoUnsaneArrayPrototype((i & 15) === 0, i);
    if ((i & 15) === 0)
        shouldBe(r[0].x, i);
    else
        shouldBe(r, i);
}
