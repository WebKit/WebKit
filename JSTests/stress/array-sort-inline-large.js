// The DFG ArraySortIntrinsic dispatcher uses `sortScratchSlotCount + 1` (17) as
// the boundary between the inline insertion-sort fast path and the DirectCall
// slow path. Arrays strictly greater than 16 must be routed to the slow path
// (arrayProtoFuncSort) via the dispatcher's `isSmall` branch.
//
// If the fast path accidentally accepted length > 16, the 16-slot scratch
// would overflow silently (ArraySortCompact would only copy 16 of N elements).
// If the slow path were mis-wired the DirectCall would sort with the wrong
// `this` or `argv` and either throw or produce a wrong result.
//
// This test covers sizes right at the boundary (17) and well above (64, 128).

function cmp(a, b) { return a - b; }
function cmpDesc(a, b) { return b - a; }

function doSort(a, c) { return a.sort(c); }

function makeReversed(n) {
    const a = new Array(n);
    for (let i = 0; i < n; ++i) a[i] = n - 1 - i;
    return a;
}

function makeRandomish(n) {
    const a = new Array(n);
    for (let i = 0; i < n; ++i) a[i] = ((i * 31 + 7) % n) - (n >> 1);
    return a;
}

function assertSorted(a, cmp) {
    for (let i = 0; i + 1 < a.length; ++i) {
        if (cmp(a[i], a[i + 1]) > 0)
            throw new Error("not sorted at index " + i + ": " + a[i] + " vs " + a[i + 1]);
    }
}

// Just above the 16 boundary: exercises the `isSmall` dispatcher's
// not-taken edge to slowBlock.
for (let w = 0; w < testLoopCount; ++w) {
    const arr = makeReversed(17);
    doSort(arr, cmp);
    assertSorted(arr, cmp);
    if (arr.length !== 17 || arr[0] !== 0 || arr[16] !== 16)
        throw new Error("len 17 wrong: " + JSON.stringify(arr));
}

// Larger sizes -- well above the boundary.
for (const size of [32, 64, 128, 256]) {
    for (let w = 0; w < testLoopCount; ++w) {
        const arr = makeReversed(size);
        doSort(arr, cmp);
        assertSorted(arr, cmp);
        if (arr[0] !== 0 || arr[size - 1] !== size - 1)
            throw new Error("len " + size + " wrong ends: " + arr[0] + ".." + arr[size - 1]);
    }
}

// Mix sort directions to ensure the slow path calls the user comparator each
// invocation (not a cached default-compare).
for (let w = 0; w < testLoopCount; ++w) {
    const arr = makeRandomish(32);
    const a1 = arr.slice();
    const a2 = arr.slice();
    doSort(a1, cmp);
    doSort(a2, cmpDesc);
    assertSorted(a1, cmp);
    assertSorted(a2, cmpDesc);
    if (a1[0] !== a2[a2.length - 1] || a1[a1.length - 1] !== a2[0])
        throw new Error("asc/desc mismatch @w=" + w);
}

// Alternating small/large at the same call site must not cause tier-down.
// mixed-sizes.js already guards this, but re-covering here with a dedicated
// large-side test catches any regression specific to the size > 16 path.
function alternating(N) {
    const small = [3, 1, 4, 1, 5, 9, 2, 6, 5, 3];
    const large = makeReversed(64);
    for (let i = 0; i < N; ++i) {
        const arr = (i & 1) ? small.slice() : large.slice();
        doSort(arr, cmp);
        assertSorted(arr, cmp);
    }
}
alternating(testLoopCount);

// Sizes that are exactly on the boundary edge: 16 (fast path) and 17 (slow path)
// interleaved. Ensures the dispatcher correctly routes both.
const atEdge = 16;
const overEdge = 17;
for (let w = 0; w < testLoopCount; ++w) {
    const at = makeReversed(atEdge);
    const over = makeReversed(overEdge);
    doSort(at, cmp);
    doSort(over, cmp);
    assertSorted(at, cmp);
    assertSorted(over, cmp);
}

// Comparator that throws on a large-array sort must propagate the exception
// from the slow-path DirectCall cleanly.
let threw = false;
try { doSort(makeReversed(32), (a, b) => { throw new Error("late"); }); }
catch (e) { threw = e.message === "late"; }
if (!threw) throw new Error("large sort lost thrown comparator error");
