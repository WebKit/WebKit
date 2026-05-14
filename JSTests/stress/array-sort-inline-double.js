// Verifies the DFG ArraySortIntrinsic's fast path handles ArrayWithDouble arrays.
// ArraySortCompact loads raw doubles from the butterfly, boxes them into the JSCellButterfly
// scratch, and ArraySortCommit unboxes them back to raw doubles when committing.

function sortIt(a, cmp) { return a.sort(cmp); }
noInline(sortIt);

function assertSameOrder(actual, expected, label, iter) {
    if (actual.length !== expected.length)
        throw new Error(`${label} iter ${iter}: length mismatch ${actual.length} != ${expected.length}`);
    for (let i = 0; i < expected.length; i++) {
        if (!Object.is(actual[i], expected[i]))
            throw new Error(`${label} iter ${iter} index ${i}: got ${actual[i]} expected ${expected[i]}`);
    }
}

// Plain double array, ascending sort.
{
    const input = [3.5, 1.2, 2.8, 0.4, 4.1, -1.5, 2.2];
    const expected = [-1.5, 0.4, 1.2, 2.2, 2.8, 3.5, 4.1];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x - y);
        assertSameOrder(r, expected, "asc", w);
        if (r !== a)
            throw new Error("asc iter " + w + ": result is not the same array");
    }
}

// Plain double array, descending sort.
{
    const input = [3.5, 1.2, 2.8, 0.4, 4.1, -1.5, 2.2];
    const expected = [4.1, 3.5, 2.8, 2.2, 1.2, 0.4, -1.5];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => y - x);
        assertSameOrder(r, expected, "desc", w);
    }
}

// Doubles with integral values (still a Double butterfly because of the mixed literal).
{
    const input = [5.0, 3.5, 1.0, 4.0, 2.5];
    const expected = [1.0, 2.5, 3.5, 4.0, 5.0];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x - y);
        assertSameOrder(r, expected, "int-like", w);
    }
}

// Negative zero and signed-ness round-trip through box/unbox.
{
    const input = [1.5, -0.0, 0.0, -1.5];
    for (let w = 0; w < testLoopCount; w++) {
        const a = input.slice();
        const r = sortIt(a, (x, y) => x - y);
        // Comparator treats -0 and +0 as equal; insertion sort is stable so original order is kept.
        if (!(r[0] === -1.5 && Object.is(r[1], -0.0) && Object.is(r[2], 0.0) && r[3] === 1.5))
            throw new Error("zero iter " + w + ": got " + r.map(x => Object.is(x, -0) ? "-0" : x).join(","));
    }
}

// Stability check: equal-key elements preserve their relative order.
{
    function buildPairs() {
        // Encode (key, tag) as key + tag/100 -> still a double, integer key part repeats.
        return [3.01, 1.01, 2.01, 1.02, 3.02, 2.02, 1.03];
    }
    const expected = [1.01, 1.02, 1.03, 2.01, 2.02, 3.01, 3.02];
    for (let w = 0; w < testLoopCount; w++) {
        const a = buildPairs();
        const r = sortIt(a, (x, y) => Math.floor(x) - Math.floor(y));
        assertSameOrder(r, expected, "stable", w);
    }
}
