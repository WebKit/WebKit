// Verifies the DFG ArraySortIntrinsic's hole handling for ArrayWithDouble arrays.
// Double butterflies represent holes as PNaN; ArraySortCompact must detect that bit pattern
// (via a NaN self-compare) and bail to the slow-path DirectCall to arrayProtoFuncSort.

function cmp(a, b) { return a - b; }
function sortIt(a) { return a.sort(cmp); }
noInline(sortIt);

// Warm up the DFG/FTL compile of sortIt with dense Double arrays.
for (let w = 0; w < testLoopCount; w++) {
    const a = [5.5, 3.5, 1.5, 4.5, 2.5];
    const r = sortIt(a);
    if (r[0] !== 1.5 || r[4] !== 5.5)
        throw new Error("dense warm-up wrong: " + JSON.stringify(r));
}

// Now feed in a holey Double array. ArraySortCompact returns the sentinel and the parser's
// branch routes to the slow path which sorts correctly.
function runHoley(iter) {
    const a = [5.5, , 3.5, , 1.5, , 4.5, , 2.5];
    const r = sortIt(a);
    if (r.length !== 9)
        throw new Error("iter " + iter + " wrong length " + r.length);
    let multiset = new Map();
    for (let i = 0; i < r.length; i++) {
        if (i in r)
            multiset.set(r[i], (multiset.get(r[i]) || 0) + 1);
    }
    for (const v of [1.5, 2.5, 3.5, 4.5, 5.5]) {
        if (multiset.get(v) !== 1)
            throw new Error("iter " + iter + " missing " + v + " (multiset = " + JSON.stringify([...multiset]) + ")");
    }
}

for (let i = 0; i < testLoopCount; i++)
    runHoley(i);
