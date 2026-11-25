function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function* reuseIteratorRegister(empty, full) {
    let count = 0;
    for (let value of empty)
        ++count;
    for (let value of full) {
        ++count;
        yield value;
    }
    yield count;
}

function* reuseIteratorRegisterReversed(full, empty) {
    let count = 0;
    for (let value of full) {
        ++count;
        yield value;
    }
    for (let value of empty)
        ++count;
    yield count;
}

async function awaitInMapLoop(map) {
    let count = 0;
    for (let entry of map)
        count += await entry[1];
    return count;
}

async function awaitInSetLoop(set) {
    let count = 0;
    for (let value of set)
        count += await value;
    return count;
}

async function addAcrossAwait(set) {
    let count = 0;
    for (let value of set) {
        await null;
        if (count++ < 3)
            set.add(value + 1);
    }
    return count;
}

function emptiedMap() {
    let map = new Map([[1, 1]]);
    map.delete(1);
    return map;
}

function emptiedSet() {
    let set = new Set([1]);
    set.delete(1);
    return set;
}

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe([...reuseIteratorRegister(new Set, new Set([1, 2]))].join(), "1,2,2");
    shouldBe([...reuseIteratorRegister(new Map, new Set([1]))].join(), "1,1");
    shouldBe([...reuseIteratorRegisterReversed(new Set([1, 2]), new Set)].join(), "1,2,2");
    shouldBe([...reuseIteratorRegisterReversed(new Set, new Set)].join(), "0");

    let promises = [
        awaitInMapLoop(new Map),
        awaitInMapLoop(emptiedMap()),
        awaitInMapLoop(new Map([[1, 2], [3, 4]])),
        awaitInSetLoop(new Set),
        awaitInSetLoop(emptiedSet()),
        awaitInSetLoop(new Set([1, 2])),
        addAcrossAwait(new Set),
        addAcrossAwait(new Set([0])),
    ];
    let results = [];
    promises.forEach((promise, index) => promise.then((value) => { results[index] = value; }, (error) => { results[index] = error; }));
    drainMicrotasks();
    shouldBe(results.join(), "0,0,6,0,0,3,0,4");
}
