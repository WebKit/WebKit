function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i)
        shouldBe(actual[i], expected[i]);
}

function collect() { return Array.prototype.slice.call(arguments); }
noInline(collect);

class Collector {
    constructor() { this.values = Array.prototype.slice.call(arguments); }
}

function spreadToArray(array) { return [...array]; }
noInline(spreadToArray);

function spreadToArrayWithOthers(array) { return [0, ...array, ...array]; }
noInline(spreadToArrayWithOthers);

function spreadToCall(array) { return collect(...array); }
noInline(spreadToCall);

function spreadToCallWithOthers(array) { return collect(0, ...array); }
noInline(spreadToCallWithOthers);

function spreadToConstruct(array) { return new Collector(...array).values; }
noInline(spreadToConstruct);

const object = { a: 1, b: 2, c: 3 };
Object.keys(object);

const factories = [
    () => ["a", 1, "b", 2.5, null, undefined],
    () => ["a", "b", "c"],
    () => [object, "b"],
    () => [...[1, 2, 3]],
    () => [...new Set(["a", 1])],
    () => [..."abc"],
    () => [...[1, , 3]],
    () => Object.keys(object),
    () => "a,b,c".split(","),
    () => ["a", 1, "b"].slice(),
    () => ["a", 1, "b"].concat([]),
];

const mutations = [
    array => { array[0] = "changed"; },
    array => { array.push("pushed"); },
    array => { array.pop(); },
    array => { array.length = 1; },
    array => { delete array[1]; },
    array => { array.reverse(); },
    array => { array.fill(42); },
    array => { array.shift(); },
    array => { array[10] = "far"; },
];

function test(factory, mutate) {
    const source = factory();
    const isCopyOnWrite = $vm.indexingMode(source) === "CopyOnWriteArrayWithContiguous";
    const expected = [];
    for (let i = 0; i < source.length; ++i)
        expected.push(source[i]);

    const first = spreadToArray(source);
    const second = spreadToArray(source);
    if (isCopyOnWrite) {
        shouldBe($vm.deltaBetweenButterflies(first, source), 0);
        shouldBe($vm.deltaBetweenButterflies(second, source), 0);
    }
    shouldBeArray(first, expected);
    shouldBeArray(spreadToArrayWithOthers(source), [0, ...expected, ...expected]);
    shouldBeArray(spreadToCall(source), expected);
    shouldBeArray(spreadToCallWithOthers(source), [0, ...expected]);
    shouldBeArray(spreadToConstruct(source), expected);

    mutate(first);
    shouldBeArray(source, expected);
    shouldBeArray(second, expected);

    mutate(source);
    shouldBeArray(second, expected);
    shouldBeArray(spreadToArray(second), expected);
    shouldBeArray(spreadToCall(second), expected);
    shouldBeArray(spreadToArray(source), first);
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test(factories[i % factories.length], mutations[i % mutations.length]);

for (const factory of factories) {
    for (const mutate of mutations)
        test(factory, mutate);
}

function keepAlive() { return spreadToArray(["a", 1, { value: 42 }]); }
noInline(keepAlive);

const kept = [];
for (let i = 0; i < 100; ++i)
    kept.push(keepAlive());
gc();
for (const array of kept) {
    shouldBe(array.length, 3);
    shouldBe(array[0], "a");
    shouldBe(array[1], 1);
    shouldBe(array[2].value, 42);
}
