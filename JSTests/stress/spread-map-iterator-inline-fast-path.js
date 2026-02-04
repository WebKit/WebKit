function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("Bad value: " + actual + " expected: " + expected);
}

function shouldBeArray(actual, expected) {
    shouldBe(actual.length, expected.length);
    for (let i = 0; i < expected.length; ++i) {
        if (actual[i] !== expected[i] && !(Number.isNaN(actual[i]) && Number.isNaN(expected[i])))
            throw new Error("Bad value at index " + i + ": " + actual[i] + " expected: " + expected[i]);
    }
}

// Fresh iterator spread (entry == 0, fast path).
{
    function testKeys(map) {
        return [...map.keys()];
    }
    function testValues(map) {
        return [...map.values()];
    }
    noInline(testKeys);
    noInline(testValues);

    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(testKeys(map), [1, 2, 3]);
        shouldBeArray(testValues(map), ["a", "b", "c"]);
    }
}

// Alternating between fast path (no deletions) and slow path (with deletions).
{
    function testKeys(map) {
        return [...map.keys()];
    }
    noInline(testKeys);

    const cleanMap = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    const dirtyMap = new Map([[10, "x"], [20, "y"], [30, "z"]]);
    dirtyMap.delete(20);

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 2 === 0) {
            shouldBeArray(testKeys(cleanMap), [1, 2, 3]);
        } else {
            shouldBeArray(testKeys(dirtyMap), [10, 30]);
        }
    }
}

// GC pressure during allocation.
{
    function testKeys(map) {
        return [...map.keys()];
    }
    noInline(testKeys);

    const map = new Map();
    for (let i = 0; i < 10; ++i)
        map.set(i, i * 10);
    const garbage = [];

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 100 === 0) {
            for (let j = 0; j < 100; ++j)
                garbage.push(new Array(1000));
            if (garbage.length > 500)
                garbage.length = 0;
        }
        const result = testKeys(map);
        shouldBe(result.length, 10);
    }
}

// Large map.
{
    function testValues(map) {
        return [...map.values()];
    }
    noInline(testValues);

    const map = new Map();
    for (let i = 0; i < 1000; ++i)
        map.set(i, i * 2);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = testValues(map);
        shouldBe(result.length, 1000);
        shouldBe(result[0], 0);
        shouldBe(result[999], 1998);
    }
}

// Empty map (edge case).
{
    function testKeys(map) {
        return [...map.keys()];
    }
    noInline(testKeys);

    const map = new Map();
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(testKeys(map), []);
    }
}

// entries() should work correctly (uses slow path since Entries kind is not inlined).
{
    function testEntries(map) {
        return [...map.entries()];
    }
    noInline(testEntries);

    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    for (let i = 0; i < testLoopCount; ++i) {
        const result = testEntries(map);
        shouldBe(result.length, 3);
        shouldBeArray(result[0], [1, "a"]);
        shouldBeArray(result[1], [2, "b"]);
        shouldBeArray(result[2], [3, "c"]);
    }
}

// entries() with deletions (slow path).
{
    function testEntries(map) {
        return [...map.entries()];
    }
    noInline(testEntries);

    const map = new Map([[1, "a"], [2, "b"], [3, "c"]]);
    map.delete(2);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = testEntries(map);
        shouldBe(result.length, 2);
        shouldBeArray(result[0], [1, "a"]);
        shouldBeArray(result[1], [3, "c"]);
    }
}

// Alternating between keys(), values(), and entries().
{
    function testKeys(map) {
        return [...map.keys()];
    }
    function testValues(map) {
        return [...map.values()];
    }
    function testEntries(map) {
        return [...map.entries()];
    }
    noInline(testKeys);
    noInline(testValues);
    noInline(testEntries);

    const map = new Map([[1, "a"], [2, "b"]]);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(testKeys(map), [1, 2]);
        shouldBeArray(testValues(map), ["a", "b"]);
        const entries = testEntries(map);
        shouldBe(entries.length, 2);
        shouldBeArray(entries[0], [1, "a"]);
        shouldBeArray(entries[1], [2, "b"]);
    }
}
