function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function* entries(map) {
    for (let [key, value] of map)
        yield key + value;
}

function countEntries(map) {
    let count = 0;
    for (let entry of entries(map))
        ++count;
    return count;
}

function* keysWithBreak(map, limit) {
    let count = 0;
    for (let [key] of map) {
        if (count++ === limit)
            return;
        yield key;
    }
}

function addWhileIterating(map) {
    let count = 0;
    for (let [key] of map) {
        if (count++ < 3)
            map.set(key + 1, 0);
    }
    return count;
}

function* addInsideGenerator(map) {
    let count = 0;
    for (let [key] of map) {
        if (count++ < 3)
            map.set(key + 1, 0);
        yield key;
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    let empty = new Map;
    shouldBe(countEntries(empty), 0);
    shouldBe(countEntries(new Map([[1, 2], [3, 4]])), 2);

    let emptied = new Map([[1, 1]]);
    emptied.delete(1);
    shouldBe(countEntries(emptied), 0);

    let later = new Map;
    shouldBe(countEntries(later), 0);
    later.set(5, 6);
    shouldBe(countEntries(later), 1);

    shouldBe([...keysWithBreak(new Map, 1)].length, 0);
    shouldBe([...keysWithBreak(new Map([[1, 0], [2, 0], [3, 0]]), 2)].length, 2);

    shouldBe(addWhileIterating(new Map), 0);
    shouldBe(addWhileIterating(new Map([[0, 0]])), 4);
    shouldBe([...addInsideGenerator(new Map)].length, 0);
    shouldBe([...addInsideGenerator(new Map([[0, 0]]))].length, 4);
}
