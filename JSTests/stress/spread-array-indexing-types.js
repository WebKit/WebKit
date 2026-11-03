function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeDenseArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i) {
        shouldBe(Object.hasOwn(actual, i), true);
        shouldBe(actual[i], expected[i]);
    }
}

function collect() { return Array.prototype.slice.call(arguments); }
noInline(collect);

function spreadToArray(iterable) { return [...iterable]; }
noInline(spreadToArray);

function spreadToCall(iterable) { return collect(...iterable); }
noInline(spreadToCall);

function spreadArrayOnlyToArray(array) { return [...array]; }
noInline(spreadArrayOnlyToArray);

function spreadArrayOnlyToCall(array) { return collect(...array); }
noInline(spreadArrayOnlyToCall);

function writable(array) {
    array[0] = array[0];
    return array;
}

function withHole(array) {
    delete array[array.length >> 1];
    return array;
}

function withLongerLength(array) {
    array.length += 3;
    return array;
}

function withArrayStorage(array) {
    $vm.ensureArrayStorage(array);
    return array;
}

function fill(array, length, callback) {
    for (let i = 0; i < length; ++i)
        array.push(callback(i));
    return array;
}

for (const helper of [writable, withHole, withLongerLength, fill]) {
    noInline(helper);
    noDFG(helper);
}

const object = { value: 42 };

const factories = [
    () => [1, 2, 3, 4],
    () => [1.5, 2.5, 3.5, 4.5],
    () => writable([1, 2, 3, 4]),
    () => writable([1.5, 2.5, 3.5, 4.5]),
    () => ["a", 1, 2.5, null, undefined],
    () => ["a", 1, 2.5, object, null, undefined],
    () => withHole([1, 2, 3, 4]),
    () => withHole([1.5, 2.5, 3.5, 4.5]),
    () => withHole(["a", 1, 2.5, object, null, undefined]),
    () => withLongerLength([1, 2, 3, 4]),
    () => withLongerLength([1.5, 2.5, 3.5, 4.5]),
    () => withLongerLength(["a", 1, 2.5, object]),
    () => [],
    () => new Array(5),
    () => withArrayStorage([1, 2, 3, 4]),
    () => withArrayStorage(withHole(["a", 1, 2.5, object])),
];

const largeFactories = [
    () => fill([], 2000, i => i),
    () => fill([], 2000, i => i + 0.5),
    () => fill([], 2000, i => "a" + i),
    () => withHole(fill([], 2000, i => i)),
    () => withHole(fill([], 2000, i => i + 0.5)),
    () => withHole(fill([], 2000, i => "a" + i)),
    () => new Array(2000),
];

const indexingModes = new Map;

function test(factory) {
    const source = factory();
    const expected = [];
    for (let i = 0; i < source.length; ++i)
        expected.push(source[i]);

    const indexingMode = $vm.indexingMode(source);
    if (!indexingModes.has(factory))
        indexingModes.set(factory, indexingMode);
    shouldBe(indexingMode, indexingModes.get(factory));

    shouldBeDenseArray(spreadToArray(source), expected);
    shouldBeDenseArray(spreadToCall(source), expected);
    shouldBeDenseArray(spreadArrayOnlyToArray(source), expected);
    shouldBeDenseArray(spreadArrayOnlyToCall(source), expected);
    shouldBeDenseArray(spreadToArray("ab"), ["a", "b"]);
    shouldBeDenseArray(spreadToCall("ab"), ["a", "b"]);
}
noInline(test);
noDFG(test);

for (let i = 0; i < testLoopCount; ++i)
    test(factories[i % factories.length]);

for (const factory of largeFactories)
    test(factory);

function keepAlive(i) { return spreadToArray([{ value: i }, "a" + i, i + 0.5]); }
noInline(keepAlive);

const kept = [];
for (let i = 0; i < 100; ++i) {
    kept.push(keepAlive(i));
    if (!(i % 10))
        edenGC();
}
gc();
for (let i = 0; i < 100; ++i) {
    shouldBe(kept[i].length, 3);
    shouldBe(kept[i][0].value, i);
    shouldBe(kept[i][1], "a" + i);
    shouldBe(kept[i][2], i + 0.5);
}
