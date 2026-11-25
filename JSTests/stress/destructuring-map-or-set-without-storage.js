function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function addToMapInTarget() {
    let map = new Map;
    let object = {};
    [object[(map.set(1, 2), "x")]] = map;
    return object.x;
}

function addToSetInTarget() {
    let set = new Set;
    let object = {};
    [object[(set.add(7), "x")]] = set;
    return object.x;
}

function* destructureAcrossYield(iterable, object) {
    [object[yield]] = iterable;
}

function addToMapAcrossYield() {
    let map = new Map;
    let object = {};
    let generator = destructureAcrossYield(map, object);
    generator.next();
    map.set(3, 4);
    generator.next("x");
    return object.x;
}

function addToSetAcrossYield() {
    let set = new Set;
    let object = {};
    let generator = destructureAcrossYield(set, object);
    generator.next();
    set.add(5);
    generator.next("x");
    return object.x;
}

function closedIterators() {
    let closed = [];
    let object = {};
    let originalReturn = Iterator.prototype.return;
    Iterator.prototype.return = function() {
        closed.push(this);
        return { };
    };
    try {
        [object[([object[(() => { throw 0; })()]] = new Map, "x")]] = new Map;
    } catch { }
    Iterator.prototype.return = originalReturn;
    return closed;
}

for (let i = 0; i < testLoopCount; ++i) {
    let entry = addToMapInTarget();
    shouldBe(entry[0], 1);
    shouldBe(entry[1], 2);
    shouldBe(addToSetInTarget(), 7);
    entry = addToMapAcrossYield();
    shouldBe(entry[0], 3);
    shouldBe(entry[1], 4);
    shouldBe(addToSetAcrossYield(), 5);
}

let closed = closedIterators();
shouldBe(closed.length, 2);
shouldBe(closed[0] === closed[1], false);
