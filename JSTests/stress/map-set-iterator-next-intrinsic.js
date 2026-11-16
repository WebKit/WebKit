function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' expected: ' + expected);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    var error = null;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        error = e;
    }
    if (!errorThrown)
        throw new Error('not thrown');
    if (String(error) !== errorMessage)
        throw new Error(`bad error: ${String(error)}`);
}

// Direct next() over Map.entries() / Map.keys() / Map.values(), including the done step.
function sumMapEntries(map) {
    var result = 0;
    var iterator = map.entries();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value[0] * 1000 + step.value[1];
    return result;
}
noInline(sumMapEntries);

function sumMapKeys(map) {
    var result = 0;
    var iterator = map.keys();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value;
    return result;
}
noInline(sumMapKeys);

function sumMapValues(map) {
    var result = 0;
    var iterator = map.values();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value;
    return result;
}
noInline(sumMapValues);

// Iterator result object shape and values, including after exhaustion.
function stepShapesMap(map) {
    var iterator = map.entries();
    var steps = [];
    for (var i = 0; i < 4; ++i) {
        var step = iterator.next();
        steps.push(step.done, Object.keys(step).join(','));
    }
    return steps;
}
noInline(stepShapesMap);

// Direct next() over Set.values() / Set.keys() / Set.entries().
function sumSetValues(set) {
    var result = 0;
    var iterator = set.values();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value;
    return result;
}
noInline(sumSetValues);

function sumSetEntries(set) {
    var result = 0;
    var iterator = set.entries();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value[0] + step.value[1];
    return result;
}
noInline(sumSetEntries);

var map = new Map();
for (var i = 0; i < 5; ++i)
    map.set(i, i * 10);

var set = new Set();
for (var i = 0; i < 5; ++i)
    set.add(i);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(sumMapEntries(map), (0 + 1 + 2 + 3 + 4) * 1000 + (0 + 10 + 20 + 30 + 40));
    shouldBe(sumMapKeys(map), 0 + 1 + 2 + 3 + 4);
    shouldBe(sumMapValues(map), 0 + 10 + 20 + 30 + 40);
    shouldBe(sumSetValues(set), 0 + 1 + 2 + 3 + 4);
    shouldBe(sumSetEntries(set), 2 * (0 + 1 + 2 + 3 + 4));

    var smallMap = new Map();
    smallMap.set("a", 1);
    smallMap.set("b", 2);
    var steps = stepShapesMap(smallMap);
    shouldBe(steps[0], false);
    shouldBe(steps[1], "value,done");
    shouldBe(steps[2], false);
    shouldBe(steps[3], "value,done");
    shouldBe(steps[4], true);
    shouldBe(steps[5], "value,done");
    shouldBe(steps[6], true);
    shouldBe(steps[7], "value,done");
}

// Empty Map / Set.
function nextEmpty(it) {
    return it.next();
}
noInline(nextEmpty);

for (var i = 0; i < 1e4; ++i) {
    var emptyMap = new Map();
    var step = nextEmpty(emptyMap.entries());
    shouldBe(step.value, undefined);
    shouldBe(step.done, true);

    var emptySet = new Set();
    step = nextEmpty(emptySet.values());
    shouldBe(step.value, undefined);
    shouldBe(step.done, true);
}

// next() with a bad |this| throws a TypeError.
var mapIteratorNext = new Map().entries().__proto__.next;
var setIteratorNext = new Set().values().__proto__.next;

function callMapNextWithBadThis(receiver) {
    return mapIteratorNext.call(receiver);
}
noInline(callMapNextWithBadThis);

function callSetNextWithBadThis(receiver) {
    return setIteratorNext.call(receiver);
}
noInline(callSetNextWithBadThis);

for (var i = 0; i < 1e4; ++i) {
    shouldThrow(() => callMapNextWithBadThis({}), 'TypeError: %MapIteratorPrototype%.next requires that |this| be a Map Iterator instance');
    shouldThrow(() => callMapNextWithBadThis(undefined), 'TypeError: %MapIteratorPrototype%.next requires that |this| be a Map Iterator instance');
    shouldThrow(() => callMapNextWithBadThis(new Set().values()), 'TypeError: %MapIteratorPrototype%.next requires that |this| be a Map Iterator instance');

    shouldThrow(() => callSetNextWithBadThis({}), 'TypeError: %SetIteratorPrototype%.next requires that |this| be a Set Iterator instance');
    shouldThrow(() => callSetNextWithBadThis(undefined), 'TypeError: %SetIteratorPrototype%.next requires that |this| be a Set Iterator instance');
    shouldThrow(() => callSetNextWithBadThis(new Map().entries()), 'TypeError: %SetIteratorPrototype%.next requires that |this| be a Set Iterator instance');
}

// Mutation during iteration via direct next() calls.
function mutateDuringIteration() {
    var m = new Map();
    m.set(1, "a");
    m.set(2, "b");
    m.set(3, "c");

    var iterator = m.entries();
    var first = iterator.next();
    shouldBe(first.value[0], 1);
    shouldBe(first.value[1], "a");

    m.delete(2);
    var second = iterator.next();
    shouldBe(second.value[0], 3);
    shouldBe(second.value[1], "c");

    m.set(4, "d");
    var third = iterator.next();
    shouldBe(third.value[0], 4);
    shouldBe(third.value[1], "d");

    var done = iterator.next();
    shouldBe(done.value, undefined);
    shouldBe(done.done, true);
}
noInline(mutateDuringIteration);

for (var i = 0; i < 1e4; ++i)
    mutateDuringIteration();

// Mixing for-of (fast iteration) and direct next() calls on the same iterator.
function mixed() {
    var m = new Map();
    m.set(1, 10);
    m.set(2, 20);
    m.set(3, 30);

    var result = 0;
    for (var [k, v] of m)
        result += k * 100 + v;

    var iterator = m.values();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value;
    return result;
}
noInline(mixed);

for (var i = 0; i < 1e4; ++i)
    shouldBe(mixed(), 100 + 200 + 300 + 10 + 20 + 30 + 10 + 20 + 30);
