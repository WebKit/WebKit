// Regression test: OSR exit inside a DFG-inlined Array.prototype.sort comparator
// must not clobber op_call's dst with the comparator's return value. Before the
// fix, exiting inside `(a, b) => a - b` and resuming in the comparator's baseline
// would make its return value flow into op_call's dst and advance past op_call,
// so `[...arr.sort(cmp)]` ended up spreading a number instead of the sorted array.

function cmp(a, b) { return a - b; }

function sortAndSpread(arr) {
    return [...arr.sort(cmp)];
}

function check(got, expected) {
    if (got.length !== expected.length)
        throw new Error("length mismatch: " + got.length + " vs " + expected.length);
    for (let i = 0; i < expected.length; ++i) {
        if (got[i] !== expected[i])
            throw new Error("element " + i + " mismatch: " + got[i] + " vs " + expected[i]);
    }
}

// Warm up with Int32 arrays so DFG inlines sort + comparator with Int32 speculation.
for (let w = 0; w < testLoopCount; ++w)
    check(sortAndSpread([5, 3, 1, 4, 2]), [1, 2, 3, 4, 5]);

// Feed doubles so the Int32-speculated subtract in the comparator OSR-exits.
for (let i = 0; i < testLoopCount; ++i)
    check(sortAndSpread([5.5, 3.25, 1.75, 4.5, 2.125]), [1.75, 2.125, 3.25, 4.5, 5.5]);

for (let i = 0; i < testLoopCount; ++i)
    check(sortAndSpread([5, 3.25, 1, 4.5, 2]), [1, 2, 3.25, 4.5, 5]);

for (let i = 0; i < testLoopCount; ++i)
    check(sortAndSpread([9.9, -1.5, 3.5, 7.0, 0.5, 4.4, 8.2]), [-1.5, 0.5, 3.5, 4.4, 7.0, 8.2, 9.9]);
