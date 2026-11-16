// Exercises the DFG ArraySortIntrinsic inline insertion sort fast path
// (length <= 2) with an inlined comparator Call, and its fallback path.

function cmpAsc(a, b) { return a - b; }

function sortWith(a) { return a.sort(cmpAsc); }

// Warm up with length-1 and length-2 Int32 arrays to profile Int32 indexing.
for (let i = 0; i < testLoopCount; i++) {
    let r1 = sortWith([42]);
    if (r1.length !== 1 || r1[0] !== 42) throw new Error("len1 wrong");
    let r2a = sortWith([1, 2]);
    if (r2a[0] !== 1 || r2a[1] !== 2) throw new Error("len2 sorted wrong");
    let r2b = sortWith([2, 1]);
    if (r2b[0] !== 1 || r2b[1] !== 2) throw new Error("len2 unsorted wrong: " + r2b);
}

// Empty array: should take the fast path too.
let r0 = sortWith([]);
if (r0.length !== 0) throw new Error("len0 wrong");

// Length > 2: must fall back to the generic C++ sort via the slow-path DirectCall.
let rBig = sortWith([5, 3, 1, 4, 2]);
for (let i = 0; i < 5; i++) if (rBig[i] !== i + 1) throw new Error("len5 wrong at " + i);

// Larger reverse-sorted.
let rev = [];
for (let i = 99; i >= 0; i--) rev.push(i);
let sorted = sortWith(rev);
for (let i = 0; i < 100; i++) if (sorted[i] !== i) throw new Error("len100 wrong at " + i);

// Comparator with side effects on a global: we still run the comparator the right number of
// times. For length 2 with elements already in order, comparator runs once and returns <= 0.
let count = 0;
function countingCmp(a, b) { count++; return a - b; }
[1, 2].sort(countingCmp);
if (count !== 1) throw new Error("expected 1 comparator call for len 2, got " + count);

count = 0;
[2, 1].sort(countingCmp);
if (count !== 1) throw new Error("expected 1 comparator call for len 2, got " + count);
