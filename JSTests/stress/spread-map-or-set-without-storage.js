function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function count(...args) {
    return args.length;
}

function spreadMapIntoArray(map) {
    return [...map].length;
}

function spreadSetIntoArray(set) {
    return [...set].length;
}

function spreadMapIntoCall(map) {
    return count(...map);
}

function spreadSetIntoCall(set) {
    return count(...set);
}

function* yieldSpreadSet(set) {
    yield [...set];
    yield count(...set);
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
    shouldBe(spreadMapIntoArray(new Map), 0);
    shouldBe(spreadMapIntoArray(emptiedMap()), 0);
    shouldBe(spreadMapIntoArray(new Map([[1, 1], [2, 2]])), 2);
    shouldBe(spreadSetIntoArray(new Set), 0);
    shouldBe(spreadSetIntoArray(emptiedSet()), 0);
    shouldBe(spreadSetIntoArray(new Set([1, 2])), 2);
    shouldBe(spreadMapIntoCall(new Map), 0);
    shouldBe(spreadMapIntoCall(emptiedMap()), 0);
    shouldBe(spreadMapIntoCall(new Map([[1, 1]])), 1);
    shouldBe(spreadSetIntoCall(new Set), 0);
    shouldBe(spreadSetIntoCall(emptiedSet()), 0);
    shouldBe(spreadSetIntoCall(new Set([1, 2, 3])), 3);

    let later = new Set;
    let results = [...yieldSpreadSet(later)];
    shouldBe(results[0].length, 0);
    shouldBe(results[1], 0);
    later.add(1);
    results = [...yieldSpreadSet(later)];
    shouldBe(results[0].length, 1);
    shouldBe(results[1], 1);
}
