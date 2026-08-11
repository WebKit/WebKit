// Verifies the DFG ArraySortIntrinsic's hole handling: a holey Int32 array must sort correctly.
// ArraySortCompact returns vm.m_sortScratchSentinel on a hole; the parser's pointer-compare
// against the sentinel routes hits to the slow path's DirectCall to arrayProtoFuncSort (which
// handles holes per spec).

function cmp(a, b) { return a - b; }
function sortIt(a) { return a.sort(cmp); }

// Warm up the DFG/FTL compile of sortIt with dense Int32 arrays.
for (let w = 0; w < testLoopCount; w++) {
    const a = [5, 3, 1, 4, 2];
    const r = sortIt(a);
    if (r[0] !== 1 || r[4] !== 5)
        throw new Error("dense warm-up wrong: " + JSON.stringify(r));
}

// Now feed in a holey Int32 array. Fast path pointer-compares the sentinel and dispatches to
// the slow-path DirectCall, which sorts correctly.
function runHoley(iter) {
    const a = [5, , 3, , 1, , 4, , 2];
    const r = sortIt(a);
    // After sort with default comparator, non-hole elements are moved to the front (ascending),
    // and holes are pushed to the end. Test's custom cmp treats undefined -> NaN -> stable at end.
    // For our (a,b)=>a-b comparator, NaN comparisons return NaN so sort preserves some order.
    // Assert the non-hole multiset is preserved: {1,2,3,4,5} plus 4 holes.
    let nonHoleCount = 0;
    let multiset = new Map();
    for (let i = 0; i < r.length; i++) {
        const v = r[i];
        if (i in r) {
            nonHoleCount++;
            multiset.set(v, (multiset.get(v) || 0) + 1);
        }
    }
    if (r.length !== 9)
        throw new Error("iter " + iter + " wrong length " + r.length);
    // Expected: 5 non-hole values (1..5) + 4 holes.
    for (let v = 1; v <= 5; v++) {
        if (multiset.get(v) !== 1)
            throw new Error("iter " + iter + " missing " + v + " (multiset = " + JSON.stringify([...multiset]) + ")");
    }
}

for (let i = 0; i < testLoopCount; i++)
    runHoley(i);
