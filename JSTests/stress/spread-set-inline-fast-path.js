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

// Alternating between fast path (no deletions) and slow path (with deletions).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const cleanSet = new Set([1, 2, 3, 4, 5]);
    const dirtySet = new Set([10, 20, 30, 40, 50]);
    dirtySet.delete(20);
    dirtySet.delete(40);

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 2 === 0) {
            shouldBeArray(test(cleanSet), [1, 2, 3, 4, 5]);
        } else {
            shouldBeArray(test(dirtySet), [10, 30, 50]);
        }
    }
}

// Repeated delete-add cycles (deletedEntryCount oscillates).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([10, 20, 30, 40, 50]);

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 3 === 0) {
            set.delete(30);
            shouldBeArray(test(set), [10, 20, 40, 50]);
            set.add(30);
        } else {
            const result = test(set);
            shouldBe(result.length, 5);
            shouldBe(result.includes(10), true);
            shouldBe(result.includes(20), true);
            shouldBe(result.includes(30), true);
            shouldBe(result.includes(40), true);
            shouldBe(result.includes(50), true);
        }
    }
}

// set.clear() then spread (null/empty storage).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set();

    for (let i = 0; i < testLoopCount; ++i) {
        set.clear();
        shouldBeArray(test(set), []);
        set.add(i);
        set.add(i + 1);
        shouldBeArray(test(set), [i, i + 1]);
    }
}

// GC pressure during allocation.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    const garbage = [];

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 100 === 0) {
            for (let j = 0; j < 100; ++j)
                garbage.push(new Array(1000));
            if (garbage.length > 500)
                garbage.length = 0;
        }
        const result = test(set);
        shouldBe(result.length, 10);
        shouldBe(result[0], 1);
        shouldBe(result[9], 10);
    }
}

// Growing set triggers rehash between spreads.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set();

    for (let i = 0; i < testLoopCount; ++i) {
        set.add(i);
        const result = test(set);
        shouldBe(result.length, set.size);
        shouldBe(result[0], 0);
        shouldBe(result[result.length - 1], i);
    }
}

// Special values: NaN, -0, +0, Infinity, -Infinity.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([NaN, 0, -0, Infinity, -Infinity, null, undefined]);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = test(set);
        shouldBe(result.length, 6);
        shouldBe(Number.isNaN(result[0]), true);
        shouldBe(Object.is(result[1], 0), true);
        shouldBe(result[2], Infinity);
        shouldBe(result[3], -Infinity);
        shouldBe(result[4], null);
        shouldBe(result[5], undefined);
    }
}

// Single element set (minimal loop iteration).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set(["only"]);

    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(test(set), ["only"]);
    }
}

// Very large set.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const elements = [];
    for (let i = 0; i < 10000; ++i)
        elements.push(i);
    const largeSet = new Set(elements);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = test(largeSet);
        shouldBe(result.length, 10000);
        shouldBe(result[0], 0);
        shouldBe(result[9999], 9999);
        shouldBe(result[42], 42);
        shouldBe(result[5678], 5678);
    }
}

// [x, ...set, y] context (NewArrayWithSpread with mixed children).
{
    function test(set) {
        return ["before", ...set, "after"];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);
    const dirtySet = new Set([10, 20, 30]);
    dirtySet.delete(20);

    for (let i = 0; i < testLoopCount; ++i) {
        if (i % 3 === 0) {
            shouldBeArray(test(dirtySet), ["before", 10, 30, "after"]);
        } else {
            shouldBeArray(test(set), ["before", 1, 2, 3, "after"]);
        }
    }
}

// Spread immediately after construction.
{
    function test(arr) {
        return [...new Set(arr)];
    }
    noInline(test);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = test([i, i + 1, i + 2, i, i + 1]);
        shouldBe(result.length, 3);
        shouldBe(result[0], i);
        shouldBe(result[1], i + 1);
        shouldBe(result[2], i + 2);
    }
}

// Multiple spreads of the same set in one expression.
{
    function test(set) {
        return [...set, ...set];
    }
    noInline(test);

    const set = new Set([1, 2, 3]);
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(test(set), [1, 2, 3, 1, 2, 3]);
    }
}

// GC between spreads with object elements (tests mutatorFence).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const obj1 = { id: 1 };
    const obj2 = { id: 2 };
    const obj3 = { id: 3 };
    const set = new Set([obj1, obj2, obj3]);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = test(set);
        shouldBe(result[0].id, 1);
        shouldBe(result[1].id, 2);
        shouldBe(result[2].id, 3);

        if (i % 500 === 0)
            gc();
    }
}

// Delete all elements one by one.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    for (let iter = 0; iter < testLoopCount; ++iter) {
        const set = new Set([1, 2, 3, 4, 5]);
        shouldBe(test(set).length, 5);
        set.delete(1);
        shouldBeArray(test(set), [2, 3, 4, 5]);
        set.delete(2);
        shouldBeArray(test(set), [3, 4, 5]);
        set.delete(3);
        shouldBeArray(test(set), [4, 5]);
        set.delete(4);
        shouldBeArray(test(set), [5]);
        set.delete(5);
        shouldBeArray(test(set), []);
    }
}

// Interleave spread with delete-add (oscillate fast/slow path).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5, 6, 7, 8]);

    for (let i = 0; i < testLoopCount; ++i) {
        const result = test(set);
        shouldBe(result.length, set.size);

        if (i % 4 === 0) {
            const first = set.values().next().value;
            set.delete(first);
            set.add(first);
        }
    }
}

// Empty set (zero-length allocation).
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set();
    for (let i = 0; i < testLoopCount; ++i) {
        shouldBeArray(test(set), []);
    }
}

// Delete then rehash to restore fast path eligibility.
{
    function test(set) {
        return [...set];
    }
    noInline(test);

    const set = new Set([1, 2, 3, 4, 5]);

    for (let i = 0; i < testLoopCount; ++i) {
        // Delete to create dirty state.
        set.delete(3);
        shouldBeArray(test(set), [1, 2, 4, 5]);

        // Add enough elements to trigger rehash (clears deletedEntryCount).
        set.add(i + 100);
        set.add(i + 200);
        set.add(i + 300);
        const result = test(set);
        shouldBe(result.length, set.size);
        shouldBe(result.includes(1), true);

        // Clean up for next iteration.
        set.delete(i + 100);
        set.delete(i + 200);
        set.delete(i + 300);
        set.add(3);
    }
}
