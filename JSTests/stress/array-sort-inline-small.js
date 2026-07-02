// Exercises the DFG ArraySortIntrinsic inline fast path for empty
// (Undecided-indexing) arrays and verifies non-empty arrays still sort
// correctly via OSR exit to the generic C++ sort.

function cmp(a, b) { return a - b; }

function callSort(a) { return a.sort(cmp); }
noInline(cmp);

// Warm up with empty arrays so DFG picks the Undecided-array fast path.
for (let i = 0; i < testLoopCount; i++) {
    let r = callSort([]);
    if (r.length !== 0) throw new Error("empty sort length != 0");
}

// A non-empty Int32 array: OSR exits from the inline fast path to baseline,
// which runs the generic C++ sort. Must produce the correct sorted output.
let r2 = callSort([2, 1]);
if (r2.length !== 2 || r2[0] !== 1 || r2[1] !== 2)
    throw new Error("length-2 fallback sort produced wrong order: " + r2);

// A 16-element reversed array: fallback must still sort correctly.
let big = [];
for (let i = 15; i >= 0; i--) big.push(i);
let rBig = callSort(big);
for (let i = 0; i < 16; i++) {
    if (rBig[i] !== i)
        throw new Error("length-16 fallback sort wrong at " + i + ": " + rBig[i]);
}

// Random-ish 100-element array for good measure.
let rand = [];
for (let i = 0; i < 100; i++) rand.push((i * 37 + 11) % 100);
let randSorted = rand.slice().sort(cmp);
for (let i = 0; i < 99; i++) {
    if (randSorted[i] > randSorted[i + 1])
        throw new Error("length-100 fallback sort not ordered at " + i);
}
