function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function* values(set) {
    for (let value of set)
        yield value;
}

function countValues(set) {
    let count = 0;
    for (let value of values(set))
        ++count;
    return count;
}

function* valuesWithBreak(set, limit) {
    let count = 0;
    for (let value of set) {
        if (count++ === limit)
            return;
        yield value;
    }
}

function addWhileIterating(set) {
    let count = 0;
    for (let value of set) {
        if (count++ < 3)
            set.add(value + 1);
    }
    return count;
}

function* addInsideGenerator(set) {
    let count = 0;
    for (let value of set) {
        if (count++ < 3)
            set.add(value + 1);
        yield value;
    }
}

for (let i = 0; i < testLoopCount; ++i) {
    let empty = new Set;
    shouldBe(countValues(empty), 0);
    shouldBe(countValues(new Set([1, 2])), 2);

    let emptied = new Set([1]);
    emptied.delete(1);
    shouldBe(countValues(emptied), 0);

    let later = new Set;
    shouldBe(countValues(later), 0);
    later.add(5);
    shouldBe(countValues(later), 1);

    shouldBe([...valuesWithBreak(new Set, 1)].length, 0);
    shouldBe([...valuesWithBreak(new Set([1, 2, 3]), 2)].length, 2);

    shouldBe(addWhileIterating(new Set), 0);
    shouldBe(addWhileIterating(new Set([0])), 4);
    shouldBe([...addInsideGenerator(new Set)].length, 0);
    shouldBe([...addInsideGenerator(new Set([0]))].length, 4);
}
