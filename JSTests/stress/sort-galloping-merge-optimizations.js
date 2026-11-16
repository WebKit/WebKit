// Comprehensive tests for PowerSort with galloping merge optimizations.
// All sorts use a JS comparator to exercise the StableSort.h code path
// (without a comparator, JSC uses a fast C++-only std::sort).

function assertSorted(array, msg) {
    for (let i = 1; i < array.length; i++) {
        if (array[i] < array[i - 1])
            throw new Error(msg + ": not sorted at index " + i + " (" + array[i - 1] + " > " + array[i] + ")");
    }
}

// Verify sort is stable: for equal .value, original .index order must be preserved.
function assertStable(array, msg) {
    for (let i = 1; i < array.length; i++) {
        if (array[i].value < array[i - 1].value)
            throw new Error(msg + ": not sorted at index " + i);
        if (array[i].value === array[i - 1].value && array[i].index < array[i - 1].index)
            throw new Error(msg + ": not stable at index " + i + " (indices " + array[i - 1].index + " -> " + array[i].index + ")");
    }
}

function assertArraysEqual(a, b, msg) {
    if (a.length !== b.length)
        throw new Error(msg + ": length mismatch " + a.length + " vs " + b.length);
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i])
            throw new Error(msg + ": mismatch at index " + i + " (" + a[i] + " vs " + b[i] + ")");
    }
}

// Verify that we have the same multiset of elements before and after sort.
function assertSameElements(original, sorted, msg) {
    let a = original.slice().sort((x, y) => x - y);
    let b = sorted.slice().sort((x, y) => x - y);
    assertArraysEqual(a, b, msg + " (element preservation)");
}

// Simple seeded PRNG for deterministic tests.
function makePRNG(seed) {
    return function() {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        return seed;
    };
}

let cmp = (a, b) => a - b;

// ============================================================================
// Section 1: extendAndNormalizeRun edge cases
// ============================================================================

// 1a. Descending run stops at equal elements (strict < required for desc detection).
// Pattern: [10,9,8,7,7,6,5,4,3,2,1,...] — the 7,7 boundary should split into
// a descending run [10,9,8,7] reversed to [7,8,9,10], then a new run starting at
// the second 7.
{
    // Build array: 100 desc, then an equal pair mid-run, then more desc, repeat.
    let a = [];
    // Segment 1: strictly descending 50..1
    for (let i = 50; i >= 1; i--) a.push(i);
    // Segment 2: ascending 51..100 (creates a run boundary at the desc→asc transition)
    for (let i = 51; i <= 100; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "desc-then-asc-boundary");
    assertSameElements(a, sorted, "desc-then-asc-boundary");
}

// 1b. Strictly descending run of exactly 2 elements (minimum descending run).
{
    // [2,1, 3,4,5,...] — run1 is [2,1] desc, reversed to [1,2], then extended asc.
    let a = [2, 1];
    for (let i = 3; i <= 100; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "desc-run-length-2");
    assertSameElements(a, sorted, "desc-run-length-2");
}

// 1c. All equal elements — descending detection must NOT trigger (uses strict <).
// A run of all-equal is ascending (a[i+1] >= a[i] since equal), so no reversal.
// Stability test: original indices must be preserved.
{
    let a = [];
    for (let i = 0; i < 200; i++)
        a.push({ value: 42, index: i });
    a.sort((x, y) => x.value - y.value);
    assertStable(a, "all-equal-200");
}

// 1d. Descending run with equal elements at the boundary.
// [10,9,8,7,7,6,5,...] — first desc run is [10,9,8,7], stops at 7==7.
// After reversal: [7,8,9,10]. Then new run starts at second 7.
{
    let a = [10, 9, 8, 7, 7, 6, 5, 4, 3, 2, 1, 0];
    // Add more to exceed extendRunCutoff
    for (let i = 11; i < 100; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "desc-equal-boundary");
    assertSameElements(a, sorted, "desc-equal-boundary");
}

// 1e. Single element at end of array — extendAndNormalizeRun returns immediately.
// This happens for the last element when it starts a new run.
{
    // Create two runs of ~64 elements + 1 trailing element.
    let a = [];
    for (let i = 0; i < 64; i++) a.push(i * 2); // even: 0,2,...,126
    for (let i = 0; i < 64; i++) a.push(i * 2 + 1); // odd: 1,3,...,127
    a.push(200); // single trailing element
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "trailing-single-element");
    assertSameElements(a, sorted, "trailing-single-element");
}

// 1f. Many short descending segments — each gets reversed independently.
{
    let a = [];
    // 20 segments of 10 descending elements each: [9,8,...,0], [19,18,...,10], ...
    for (let seg = 0; seg < 20; seg++) {
        let base = seg * 10;
        for (let i = 9; i >= 0; i--)
            a.push(base + i);
    }
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "many-short-desc-segments");
    assertSameElements(a, sorted, "many-short-desc-segments");
}

// ============================================================================
// Section 2: Pre-merge trimming in mergeRuns
// ============================================================================

// 2a. Full left trim: all elements in left run <= first element of right run.
// After trim, leftLength=0 → just copy right, skip merge entirely.
{
    let a = [];
    for (let i = 0; i < 100; i++) a.push(i);       // run 1: [0..99]
    for (let i = 100; i < 200; i++) a.push(i);      // run 2: [100..199]
    // But these form one big ascending run! Break it:
    // Use non-overlapping desc + asc to force two runs.
    a = [];
    for (let i = 99; i >= 0; i--) a.push(i);        // desc run → reversed to [0..99]
    for (let i = 100; i < 200; i++) a.push(i);       // asc run [100..199]
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "full-left-trim");
    assertSameElements(a, sorted, "full-left-trim");
}

// 2b. Full right trim: all elements in right run >= last element of left run.
// After trim, rightLength=0 → just copy left, skip merge entirely.
{
    let a = [];
    for (let i = 0; i < 100; i++) a.push(i);        // asc run [0..99]
    for (let i = 199; i >= 100; i--) a.push(i);      // desc run → reversed to [100..199]
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "full-right-trim");
    assertSameElements(a, sorted, "full-right-trim");
}

// 2c. No trim at all: runs completely overlap in value range.
// gallopRight(B[0], A) returns 0, gallopLeft(A[last], B) returns rightLength.
{
    let a = [];
    // Run 1: even numbers [0,2,4,...,198] (100 elements)
    for (let i = 0; i < 100; i++) a.push(i * 2);
    // Run 2: odd numbers [1,3,5,...,199] (100 elements)
    // Must start descending or have a break to create a new run.
    // Since 1 < 198, it's a break. But we want it ascending.
    // 1 < a[99]=198, so comparator(1, 198) = true → new run starts.
    for (let i = 0; i < 100; i++) a.push(i * 2 + 1);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "no-trim-interleaved");
    assertSameElements(a, sorted, "no-trim-interleaved");
}

// 2d. Partial trim on both sides: runs overlap in the middle.
// Left=[0..99], Right=[50..149]. skipLeft skips [0..49], skipRight skips [100..149].
// Merge only needs to handle [50..99] vs [50..99].
{
    let a = [];
    for (let i = 0; i < 100; i++) a.push(i);           // asc run [0..99]
    // Force new run: start with something < 99
    for (let i = 50; i < 150; i++) a.push(i);           // should start new run since 50 < 99
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "partial-trim-both-sides");
    assertSameElements(a, sorted, "partial-trim-both-sides");
}

// 2e. Partial left trim only: left run's tail overlaps fully with right.
// Left=[0..49, 200..249], Right=[100..199].
// gallopRight(100, Left) → skips [0..49] (50 elements).
// gallopLeft(249, Right) → 249 > all of Right, no skip.
{
    let a = [];
    // Run 1: [0..49, 200..249] (ascending since 200 > 49)
    for (let i = 0; i < 50; i++) a.push(i);
    for (let i = 200; i < 250; i++) a.push(i);
    // Run 2: [100..199] (new run since 100 < 249)
    for (let i = 100; i < 200; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "partial-left-trim-only");
    assertSameElements(a, sorted, "partial-left-trim-only");
}

// ============================================================================
// Section 3: Galloping mode
// ============================================================================

// 3a. Trigger galloping via rightWins >= minGallop (7).
// Run 1 has large values, Run 2 starts with many small values.
// After pre-merge trim, right elements are all smaller → right wins consecutively.
{
    let a = [];
    // Run 1: [200,201,...,299] (100 ascending)
    for (let i = 200; i < 300; i++) a.push(i);
    // Run 2: [0,1,...,49, 250,251,...,299] (100 ascending, since 250 > 49)
    // This starts a new run because 0 < 299.
    for (let i = 0; i < 50; i++) a.push(i);
    for (let i = 250; i < 300; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallop-right-wins");
    assertSameElements(a, sorted, "gallop-right-wins");
}

// 3b. Trigger galloping via leftWins >= minGallop.
// Run 1 starts with many small values, Run 2 has large values.
{
    let a = [];
    // Run 1: [0,1,...,49, 250,251,...,299] (ascending)
    for (let i = 0; i < 50; i++) a.push(i);
    for (let i = 250; i < 300; i++) a.push(i);
    // Run 2: [200,201,...,299] (new run since 200 < 299)
    for (let i = 200; i < 300; i++) a.push(i);
    // After trim: gallopRight(B[0]=200, A) → 200 is after [0..49] = 50.
    // Remaining A = [250,...,299]. gallopLeft(A[last]=299, B) → all of B ≤ 299, skipRight=0.
    // Merge A'=[250,...,299] vs B=[200,...,299].
    // 200 < 250 → right wins. 201 < 250 → right wins. ... 249 < 250 → right wins. (50 times!)
    // Actually wait: after trim A' starts at 250, B starts at 200.
    // compare(B[0]=200, A'[0]=250): 200 < 250 → right wins.
    // This continues until B reaches 250+.
    // So rightWins reaches 50, well past minGallop=7.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallop-left-wins");
    assertSameElements(a, sorted, "gallop-left-wins");
}

// 3c. Galloping mode entry, then exit back to linear merge.
// Pattern: clustered start (triggers galloping), then interleaved (exits galloping).
{
    let a = [];
    // Run 1: [0,1,...,19, 30,32,34,...,58] — 20 clustered then 15 sparse
    for (let i = 0; i < 20; i++) a.push(i);
    for (let i = 0; i < 15; i++) a.push(30 + i * 2);
    // Run 2: [20,21,...,29, 31,33,35,...,59] — 10 clustered then 15 sparse interleaved
    for (let i = 20; i < 30; i++) a.push(i);
    for (let i = 0; i < 15; i++) a.push(31 + i * 2);
    // After trim: gallopRight(20, Run1) → skip [0..19]. Remaining A=[30,32,...,58].
    // gallopLeft(58, Run2=[20,...,29,31,...,59]) → 58 < 59, position is before 59.
    // skipRight=1 (just 59). Remaining B=[20,...,29,31,...,57].
    // Merge A=[30,32,...,58] vs B=[20,...,29,31,...,57]:
    //   B[0]=20 < A[0]=30 → right. B[1]=21 → right. ... B[9]=29 → right. (10 wins, gallop!)
    //   In galloping: gallopRight(30, B[10:]) → finds 30 in [31,33,...]. 30 < 31, k=0. Copy B[10]=31... wait no.
    //   Then gallopLeft(A[0]=30, B[10:]=[31,33,...]) → 30 < 31, k=0. Copy A[0]=30.
    //   Both k=0 < 7 → exit galloping.
    //   Back to linear: A[1]=32 vs B[10]=31 → left=32 > right=31, right wins. B[11]=33 vs A[1]=32 → left wins. Alternating.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallop-then-linear");
    assertSameElements(a, sorted, "gallop-then-linear");
}

// 3d. Galloping where gallopRight finds many left elements to bulk-copy.
// Need: right element is large, many left elements smaller.
{
    let a = [];
    // Run 1: [0,1,...,99, 500,501,...,509] (110 elements)
    for (let i = 0; i < 100; i++) a.push(i);
    for (let i = 500; i < 510; i++) a.push(i);
    // Run 2: [50,51,...,149, 510,511,...,519] (110 elements, starts at 50 < 509)
    for (let i = 50; i < 150; i++) a.push(i);
    for (let i = 510; i < 520; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallop-bulk-copy-left");
    assertSameElements(a, sorted, "gallop-bulk-copy-left");
}

// 3e. Galloping where gallopLeft finds many right elements to bulk-copy.
{
    let a = [];
    // Run 1: [500,501,...,509, 0,1,...,9] — desc then asc? No, need ascending.
    // Run 1: [0,1,...,9, 500,501,...,509] (ascending, 20 elements)
    for (let i = 0; i < 10; i++) a.push(i);
    for (let i = 500; i < 510; i++) a.push(i);
    // Run 2: [10,11,...,109] (100 elements, new run since 10 < 509)
    for (let i = 10; i < 110; i++) a.push(i);
    // After trim: gallopRight(10, Run1) → 10. Skip [0..9]. Remaining A=[500,...,509].
    // gallopLeft(509, Run2=[10,...,109]) → all < 509, returns 100. skipRight=0.
    // Merge A=[500,...,509] vs B=[10,...,109]:
    // B[0]=10 < A[0]=500 → right wins. Continues for 100 elements.
    // In galloping: gallopRight(500, B[7:]) → all of B < 500, returns large k.
    // Then gallopLeft(A[0]=500, B_remaining) → copies rest of B.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallop-bulk-copy-right");
    assertSameElements(a, sorted, "gallop-bulk-copy-right");
}

// 3f. Left run exhausted during galloping mode (goto mergeFinish from gallop).
{
    let a = [];
    // Run 1: [100, 101] (short, will be boosted to 64 via insertion sort)
    // Actually, let's use longer runs for predictability.
    // Run 1: [100,101,...,109] (10 ascending)
    for (let i = 100; i < 110; i++) a.push(i);
    // Run 2: [0,1,...,99] (100 ascending, starts at 0 < 109)
    for (let i = 0; i < 100; i++) a.push(i);
    // After trim: gallopRight(0, Run1=[100,...,109]) → 0, no skip.
    // gallopLeft(109, Run2=[0,...,99]) → all < 109, returns 100. skipRight=0.
    // Merge A=[100,...,109] vs B=[0,...,99]:
    // B[0]=0 < A[0]=100 → right wins 7+ times → galloping.
    // gallopRight(src[right], A) → all of A > src[right], k=0.
    // Copy B element. gallopLeft(A[0]=100, B_remaining) → copies many B elements.
    // Eventually right exhausted → mergeFinish copies remaining A.

    // But wait — with short runs and insertion sort boosting...
    // Run1 is 10 elements, < extendRunCutoff? No, extendRunCutoff=8, 10 >= 8.
    // But 10 < forceRunLength=64... actually the boost only happens if run < extendRunCutoff.
    // So run of 10 is kept as-is.
    // Actually, insertion sort boost: "if (run1.m_end - run1.m_begin < extendRunCutoff)"
    // extendRunCutoff=8. 10-element run: end-begin=9 >= 8. No boost. Good.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "left-exhausted-in-gallop");
    assertSameElements(a, sorted, "left-exhausted-in-gallop");
}

// 3g. Right run exhausted during galloping mode.
{
    let a = [];
    // Run 1: [0,1,...,99] (100 ascending)
    for (let i = 0; i < 100; i++) a.push(i);
    // Run 2: [100,101,...,109] (10 ascending, 100 < 99 is false so... wait, 100 > 99, so
    // comparator(100, 99) = (100 < 99) = false → extends run1! They merge into one run.
    // I need a break. Run2 must start with value < Run1's last value.
    // Run 2: [50,51,...,59] (10 ascending, 50 < 99 → new run)
    for (let i = 50; i < 60; i++) a.push(i);
    // After trim: gallopRight(50, Run1=[0,...,99]) → 50. Skip [0..49]. Remaining A=[50,...,99].
    // gallopLeft(99, Run2=[50,...,59]) → all ≤ 99, returns 10. skipRight=0.
    // Merge A=[50,...,99] vs B=[50,...,59]:
    // A[0]=50 == B[0]=50 → left wins (equal → !(right < left) → take left).
    // A[1]=51 vs B[0]=50 → left=51 > right=50, right wins.
    // A[1]=51 vs B[1]=51 → left wins.
    // Alternating, then B runs out → mergeFinish copies remaining A.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "right-exhausted-in-gallop");
    assertSameElements(a, sorted, "right-exhausted-in-gallop");
}

// 3h. minGallop adaptation: force it to decrease to 1 via sustained galloping.
// Create a pattern where galloping is very productive over many merges.
{
    let a = [];
    // Multiple non-overlapping sorted segments separated by breaks.
    // Each pair of segments: one high, one low → galloping always productive.
    for (let seg = 0; seg < 8; seg++) {
        let base = seg * 200;
        // High segment: [base+100, ..., base+199] (100 elements)
        for (let i = 0; i < 100; i++) a.push(base + 100 + i);
        // Low segment starting new run: [base, ..., base+99]
        for (let i = 0; i < 100; i++) a.push(base + i);
    }
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "mingallop-decrease");
    assertSameElements(a, sorted, "mingallop-decrease");
}

// 3i. minGallop adaptation: force it to increase by entering then quickly exiting galloping.
// Interleaved data where galloping doesn't help.
{
    let a = [];
    // Run 1: even numbers [0,2,4,...,998]
    for (let i = 0; i < 500; i++) a.push(i * 2);
    // Run 2: odd numbers [1,3,5,...,999]
    for (let i = 0; i < 500; i++) a.push(i * 2 + 1);
    // These two runs perfectly interleave → no galloping benefit.
    // After pre-merge trim: gallopRight(1, [0,2,...,998]) = 1, skipLeft=1.
    // gallopLeft(998, [1,3,...,999]) → 998 < 999, position=499, skipRight=1.
    // Linear merge alternates: never reaches 7 consecutive wins.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "mingallop-increase-interleaved");
    assertSameElements(a, sorted, "mingallop-increase-interleaved");
}

// ============================================================================
// Section 4: arrayStableSort structure
// ============================================================================

// 4a. Entire array is one ascending run — no merge needed.
{
    let a = [];
    for (let i = 0; i < 500; i++) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertArraysEqual(sorted, a, "single-ascending-run");
}

// 4b. Entire array is one descending run — reversed, no merge needed.
{
    let a = [];
    for (let i = 499; i >= 0; i--) a.push(i);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "single-descending-run");
    if (sorted[0] !== 0 || sorted[499] !== 499)
        throw new Error("single-descending-run: wrong boundaries");
}

// 4c. Short run boosting: first run is very short (< extendRunCutoff=8),
// so it gets insertion-sorted up to forceRunLength=64 elements.
{
    let a = [];
    // Start with 3-element descending: [2,1,0] → reversed to [0,1,2], then boosted to 64.
    a.push(2, 1, 0);
    // Fill with random-ish data to make the insertion sort work.
    let rng = makePRNG(42);
    for (let i = 3; i < 200; i++)
        a.push(rng() % 500);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "short-run-boost");
    assertSameElements(a, sorted, "short-run-boost");
}

// 4d. Second run is short and gets boosted.
{
    let a = [];
    // Long first run: 100 ascending
    for (let i = 0; i < 100; i++) a.push(i);
    // Short second run: 3 elements descending [102,101,100] → reversed to [100,101,102].
    // Then boosted via insertion sort to min(64, remaining).
    a.push(102, 101, 100);
    // More random data for insertion sort to absorb.
    let rng = makePRNG(99);
    for (let i = 0; i < 100; i++)
        a.push(rng() % 500);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "second-run-short-boost");
    assertSameElements(a, sorted, "second-run-short-boost");
}

// 4e. Run extension after insertion sort: after boosting a short run to 64 elements
// via insertion sort, the algorithm tries to extend further.
{
    let a = [];
    // Short desc run [5,4,3,2,1] (5 elements, < 8).
    for (let i = 5; i >= 1; i--) a.push(i);
    // After reversal: [1,2,3,4,5]. Boosted to 64 via insertion sort.
    // Then extension: if element 65 >= element 64, run extends.
    // Make elements 6-100 ascending so extension works.
    for (let i = 6; i <= 100; i++) a.push(i);
    // This means the entire array becomes one run (no merge).
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "run-extension-after-boost");
    assertSameElements(a, sorted, "run-extension-after-boost");
}

// 4f. Boundary: exactly extendRunCutoff (8) element run — not boosted.
{
    let a = [];
    for (let i = 7; i >= 0; i--) a.push(i); // 8 desc → reversed to [0..7]
    for (let i = 100; i >= 8; i--) a.push(i); // another desc run → reversed to [8..100]
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "exactly-extendRunCutoff");
    assertSameElements(a, sorted, "exactly-extendRunCutoff");
}

// 4g. Boundary: 7-element run (< extendRunCutoff=8) — gets boosted.
{
    let a = [];
    for (let i = 6; i >= 0; i--) a.push(i); // 7 desc → reversed to [0..6], then boosted
    let rng = makePRNG(77);
    for (let i = 7; i < 200; i++)
        a.push(rng() % 500);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "below-extendRunCutoff");
    assertSameElements(a, sorted, "below-extendRunCutoff");
}

// 4h. Power stack cascading merges: many runs cause multiple stack pops.
{
    let a = [];
    // Create many short runs by alternating asc/desc segments of ~10 elements.
    for (let seg = 0; seg < 30; seg++) {
        let base = seg * 20;
        if (seg % 2 === 0) {
            for (let i = 0; i < 10; i++) a.push(base + i);
        } else {
            for (let i = 9; i >= 0; i--) a.push(base + i);
        }
    }
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "cascading-power-merges");
    assertSameElements(a, sorted, "cascading-power-merges");
}

// 4i. Final stack drain: multiple entries on power stack at the end.
{
    let a = [];
    // Ascending runs of increasing size — the power function assigns different
    // power values, so they accumulate on the stack.
    let pos = 0;
    for (let runLen of [10, 20, 15, 25, 12, 18, 30, 8, 22, 14]) {
        // Alternate between asc and desc to force run boundaries.
        if (pos > 0) {
            // Start desc (value < previous last) to break previous run.
            for (let i = runLen - 1; i >= 0; i--)
                a.push(pos + i);
        } else {
            for (let i = 0; i < runLen; i++)
                a.push(pos + i);
        }
        pos += runLen;
    }
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "final-stack-drain");
    assertSameElements(a, sorted, "final-stack-drain");
}

// ============================================================================
// Section 5: gallopLeft and gallopRight edge cases
// ============================================================================

// 5a. gallopRight: key smaller than all elements → returns 0.
// This happens in pre-merge trim when B[0] < A[0].
{
    let a = [];
    // Run 1: [100,...,199] (ascending)
    for (let i = 100; i < 200; i++) a.push(i);
    // Run 2: [0,...,99] (starts at 0 < 199, new run)
    for (let i = 0; i < 100; i++) a.push(i);
    // gallopRight(B[0]=0, A=[100,...,199]) → 0.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopRight-key-before-all");
    assertSameElements(a, sorted, "gallopRight-key-before-all");
}

// 5b. gallopRight: key larger than all elements → returns length.
// This happens in pre-merge trim when B[0] > A[last].
{
    let a = [];
    // Run 1: [0,...,99]
    for (let i = 0; i < 100; i++) a.push(i);
    // Run 2: needs to start a new run. Use desc to break.
    // [199,...,100] → reversed to [100,...,199]
    for (let i = 199; i >= 100; i--) a.push(i);
    // gallopRight(B[0]=100, A=[0,...,99]) → 100 (after all, since 100 > 99).
    // Full left trim. No actual merge.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopRight-key-after-all");
    assertSameElements(a, sorted, "gallopRight-key-after-all");
}

// 5c. gallopLeft: key larger than all elements → returns length.
// This happens in pre-merge right trim when A[last] > B[last].
{
    let a = [];
    // Run 1: [0,...,99, 300,...,399] (ascending)
    for (let i = 0; i < 100; i++) a.push(i);
    for (let i = 300; i < 400; i++) a.push(i);
    // Run 2: [100,...,199] (starts at 100 < 399)
    for (let i = 100; i < 200; i++) a.push(i);
    // gallopLeft(A[last]=399, B=[100,...,199]) → all < 399, returns 100. skipRight=0.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopLeft-key-after-all");
    assertSameElements(a, sorted, "gallopLeft-key-after-all");
}

// 5d. gallopLeft: key smaller than all elements → returns 0.
{
    let a = [];
    // Run 1: [0,...,9] (10 ascending)
    for (let i = 0; i < 10; i++) a.push(i);
    // Run 2: [5,...,104] (starts at 5 < 9, new run)
    for (let i = 5; i < 105; i++) a.push(i);
    // gallopLeft(A[last]=9, B=[5,...,104], hint=rightLen-1=99):
    // B[99]=104 >= 9, so gallop left from hint=99. Finds position 5 (before B[5]=10).
    // skipRight = 100 - 5 = 95. Remaining B=[5,...,9].
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopLeft-key-before-some");
    assertSameElements(a, sorted, "gallopLeft-key-before-some");
}

// 5e. gallopLeft with hint at end, galloping left through many elements.
// The exponential search should hit maxOffset cap.
{
    let a = [];
    // Run 1: [0, 1000] (ascending, 2 elements — too short, gets boosted)
    // Let's use a pattern that forces gallopLeft to search far from hint.
    // Run 1: [0,...,49, 500] (51 ascending)
    for (let i = 0; i < 50; i++) a.push(i);
    a.push(500);
    // Run 2: [50,...,499] (starts at 50 < 500, new run)
    for (let i = 50; i < 500; i++) a.push(i);
    // gallopLeft(500, B=[50,...,499], hint=449):
    // B[449]=499 < 500, so hintLessThanKey=true. Gallop right from 449.
    // maxOffset = 450 - 449 = 1. offset=1 → 1 < 1 false. lastOffset=449, offset=450.
    // Binary: one step. Returns 450. skipRight = 450 - 450 = 0.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopLeft-hint-far");
    assertSameElements(a, sorted, "gallopLeft-hint-far");
}

// 5f. gallopRight: key equals many elements — should return after all equal elements.
{
    let a = [];
    // Run 1: [5,5,5,...,5, 10,10,...,10] (50 fives then 50 tens)
    for (let i = 0; i < 50; i++) a.push(5);
    for (let i = 0; i < 50; i++) a.push(10);
    // Run 2: [5,5,...,5, 10,10,...,10] (same pattern, new run since 5 < 10)
    for (let i = 0; i < 50; i++) a.push(5);
    for (let i = 0; i < 50; i++) a.push(10);
    // gallopRight(B[0]=5, A=[5*50, 10*50]) → should return 50 (after all 5s in A).
    // This is the rightmost insertion point for 5.
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopRight-many-equals");
    // All 5s should come before all 10s.
    let fives = sorted.filter(x => x === 5);
    let tens = sorted.filter(x => x === 10);
    if (fives.length !== 100 || tens.length !== 100)
        throw new Error("gallopRight-many-equals: wrong count");
}

// 5g. gallopLeft: key equals many elements — should return before all equal elements.
{
    let a = [];
    // Run 1: [1,...,49, 50, 50, ..., 50] (49 unique + 51 fifties)
    for (let i = 1; i < 50; i++) a.push(i);
    for (let i = 0; i < 51; i++) a.push(50);
    // Run 2: [25,...,75] (starts at 25 < 50, new run)
    for (let i = 25; i <= 75; i++) a.push(i);
    // gallopLeft(A[last]=50, B, hint=rightLen-1):
    // Should return position before all 50s in B (leftmost insertion point).
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "gallopLeft-many-equals");
    assertSameElements(a, sorted, "gallopLeft-many-equals");
}

// ============================================================================
// Section 6: Stability tests
// ============================================================================

// 6a. Stability through descending run reversal with duplicate keys.
{
    let a = [];
    // Descending run with duplicate keys: must use strict < for descending.
    // [5,4,3,3,2,1] — the 3,3 pair stops desc extension.
    // After: [3,4,5] reversed from [5,4,3], then new run [3,2,1] detected.
    // But [3,2,1] is desc → reversed to [1,2,3].
    // The two 3s are now in different runs. Their stability depends on merge.
    // The 3 from run1 (original index 2) should come before 3 from run2 (original index 3).
    let vals = [];
    for (let i = 50; i >= 1; i--) vals.push(i);
    // Add duplicates at various points
    for (let i = 1; i <= 50; i++) vals.push(i);
    let wrapped = vals.map((v, i) => ({ value: v, index: i }));
    wrapped.sort((x, y) => x.value - y.value);
    assertStable(wrapped, "stability-desc-reversal-dupes");
}

// 6b. Stability through galloping merge with many duplicate keys.
{
    let a = [];
    // 200 elements with only 5 distinct keys, ensuring lots of equal comparisons.
    for (let i = 0; i < 200; i++)
        a.push({ value: i % 5, index: i });
    a.sort((x, y) => x.value - y.value);
    assertStable(a, "stability-gallop-merge-dupes");
}

// 6c. Stability across multiple merge levels with duplicates.
{
    let a = [];
    // Many small runs, each containing overlapping values.
    for (let seg = 0; seg < 20; seg++) {
        let base = (seg % 3) * 10; // only 3 distinct bases: 0, 10, 20
        // Descending segment: creates a new run
        for (let i = 14; i >= 0; i--)
            a.push({ value: base + i, index: a.length });
    }
    a.sort((x, y) => x.value - y.value);
    assertStable(a, "stability-multi-level-merge");
}

// 6d. Stability: concatenated sorted subarrays with same values.
{
    let a = [];
    for (let batch = 0; batch < 5; batch++) {
        for (let i = 0; i < 100; i++)
            a.push({ value: i, index: a.length });
    }
    a.sort((x, y) => x.value - y.value);
    assertStable(a, "stability-concat-same-values");
}

// ============================================================================
// Section 7: Exception safety
// ============================================================================

// 7a. Exception during descending run detection.
{
    let a = [5, 4, 3, 2, 1, 0];
    for (let i = 6; i < 100; i++) a.push(i);
    let callCount = 0;
    let threw = false;
    try {
        a.sort((x, y) => {
            callCount++;
            if (callCount === 3)
                throw new Error("during-desc-detection");
            return x - y;
        });
    } catch (e) {
        if (e.message === "during-desc-detection") threw = true;
        else throw e;
    }
    if (!threw)
        throw new Error("exception during desc detection not propagated");
}

// 7b. Exception during pre-merge gallopRight.
{
    let a = [];
    // Two runs that will be merged.
    for (let i = 99; i >= 0; i--) a.push(i);   // desc → reversed to [0..99]
    for (let i = 50; i < 150; i++) a.push(i);   // asc [50..149]
    let callCount = 0;
    let threw = false;
    try {
        a.sort((x, y) => {
            callCount++;
            // Throw during the merge phase (after run detection is done).
            // Run detection takes ~200 comparisons, merge starts after.
            if (callCount > 250)
                throw new Error("during-gallop-trim");
            return x - y;
        });
    } catch (e) {
        if (e.message === "during-gallop-trim") threw = true;
        else throw e;
    }
    if (!threw)
        throw new Error("exception during gallop trim not propagated");
}

// 7c. Exception during galloping mode.
{
    let a = [];
    // Pattern that triggers galloping: Run1=[200,...,299], Run2=[0,...,49,250,...,299]
    for (let i = 200; i < 300; i++) a.push(i);
    for (let i = 0; i < 50; i++) a.push(i);
    for (let i = 250; i < 300; i++) a.push(i);
    let callCount = 0;
    let threw = false;
    try {
        a.sort((x, y) => {
            callCount++;
            if (callCount > 300)
                throw new Error("during-galloping");
            return x - y;
        });
    } catch (e) {
        if (e.message === "during-galloping") threw = true;
        else throw e;
    }
    if (!threw)
        throw new Error("exception during galloping not propagated");
}

// ============================================================================
// Section 8: Size boundary tests
// ============================================================================

// 8a. Sizes around insertion sort threshold (extendRunCutoff=8).
{
    for (let n = 1; n <= 20; n++) {
        let a = [];
        for (let i = n; i >= 1; i--) a.push(i);
        let sorted = a.slice().sort(cmp);
        assertSorted(sorted, "size-" + n);
        if (sorted.length !== n)
            throw new Error("size-" + n + ": wrong length");
    }
}

// 8b. Size exactly forceRunLength (64): one run after boosting, no merge.
{
    let rng = makePRNG(123);
    let a = [];
    for (let i = 0; i < 64; i++) a.push(rng() % 1000);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "size-64-forceRunLength");
    assertSameElements(a, sorted, "size-64-forceRunLength");
}

// 8c. Size 65: two runs (first boosted to 64, remainder is 1 element).
{
    let rng = makePRNG(456);
    let a = [];
    for (let i = 0; i < 65; i++) a.push(rng() % 1000);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "size-65-two-runs");
    assertSameElements(a, sorted, "size-65-two-runs");
}

// 8d. Size 128: two runs of 64.
{
    let rng = makePRNG(789);
    let a = [];
    for (let i = 0; i < 128; i++) a.push(rng() % 1000);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "size-128-two-runs");
    assertSameElements(a, sorted, "size-128-two-runs");
}

// 8e. Size 129: three runs.
{
    let rng = makePRNG(321);
    let a = [];
    for (let i = 0; i < 129; i++) a.push(rng() % 1000);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "size-129-three-runs");
    assertSameElements(a, sorted, "size-129-three-runs");
}

// ============================================================================
// Section 9: Complex patterns
// ============================================================================

// 9a. Organ pipe: ascending then descending.
{
    let a = [];
    for (let i = 0; i < 200; i++) a.push(i);
    for (let i = 199; i >= 0; i--) a.push(i);
    let wrapped = a.map((v, i) => ({ value: v, index: i }));
    wrapped.sort((x, y) => x.value - y.value);
    assertStable(wrapped, "organ-pipe");
}

// 9b. Reverse organ pipe: descending then ascending.
{
    let a = [];
    for (let i = 199; i >= 0; i--) a.push(i);
    for (let i = 0; i < 200; i++) a.push(i);
    let wrapped = a.map((v, i) => ({ value: v, index: i }));
    wrapped.sort((x, y) => x.value - y.value);
    assertStable(wrapped, "reverse-organ-pipe");
}

// 9c. Sawtooth with varying segment lengths.
{
    let a = [];
    let pos = 0;
    for (let segLen of [100, 50, 200, 30, 150, 80, 10, 120]) {
        if (a.length > 0 && a.length % 2 === 0) {
            // Descending segment
            for (let i = segLen - 1; i >= 0; i--)
                a.push(pos + i);
        } else {
            // Ascending segment
            for (let i = 0; i < segLen; i++)
                a.push(pos + i);
        }
        pos += segLen;
    }
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "sawtooth-varying-lengths");
    assertSameElements(a, sorted, "sawtooth-varying-lengths");
}

// 9d. Plateau: long run of equal values surrounded by sorted sequences.
{
    let a = [];
    for (let i = 0; i < 100; i++) a.push(i);
    for (let i = 0; i < 200; i++) a.push(100); // plateau
    for (let i = 101; i < 200; i++) a.push(i);
    // [0..99] is one run. [100*200] is one run (ascending, all equal).
    // But 100 == 99+1, so comparator(100, 99) = false → extends run 1!
    // Similarly [100, 100, ...] extends. Then 101 > 100, extends further.
    // Actually the entire array might be one run! Let me break it.
    // Better: force a break.
    a = [];
    for (let i = 0; i < 100; i++) a.push(i);           // asc [0..99]
    for (let i = 0; i < 200; i++) a.push(50);           // starts at 50 < 99, new run
    for (let i = 51; i < 150; i++) a.push(i);           // continues ascending from 51
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "plateau-in-middle");
    assertSameElements(a, sorted, "plateau-in-middle");
}

// 9e. Two-value array: only 0s and 1s.
{
    let a = [];
    let rng = makePRNG(555);
    for (let i = 0; i < 500; i++)
        a.push(rng() % 2);
    let sorted = a.slice().sort(cmp);
    assertSorted(sorted, "two-values");
    let zeros = sorted.filter(x => x === 0).length;
    let ones = sorted.filter(x => x === 1).length;
    if (zeros + ones !== 500)
        throw new Error("two-values: element count wrong");
}

// 9f. Already sorted with duplicates — pre-merge trim should handle everything.
{
    let a = [];
    for (let i = 0; i < 100; i++) {
        a.push(i);
        a.push(i); // duplicate each
    }
    // This is one ascending run (duplicates are >=). No merge.
    // Break into two runs:
    let b = [];
    for (let i = 0; i < 100; i++) b.push(i);
    for (let i = 0; i < 100; i++) b.push(i); // starts at 0 < 99, new run
    let sorted = b.slice().sort(cmp);
    assertSorted(sorted, "sorted-with-duplicates");
    assertSameElements(b, sorted, "sorted-with-duplicates");
}

// 9g. Large random test with stability check.
{
    let rng = makePRNG(12345);
    let a = [];
    for (let i = 0; i < 10000; i++)
        a.push({ value: rng() % 100, index: i });
    a.sort((x, y) => x.value - y.value);
    assertStable(a, "large-random-stability");
}

// ============================================================================
// Section 10: TypedArray paths
// ============================================================================

// 10a. TypedArray with descending run detection.
{
    let ta = new Int32Array(500);
    for (let i = 0; i < 500; i++) ta[i] = 500 - i;
    ta.sort((x, y) => x - y);
    for (let i = 1; i < ta.length; i++) {
        if (ta[i] < ta[i - 1])
            throw new Error("typed-array-desc: not sorted at " + i);
    }
}

// 10b. TypedArray with galloping pattern.
{
    let ta = new Float64Array(200);
    // Run 1: [200,...,299]
    for (let i = 0; i < 100; i++) ta[i] = 200 + i;
    // Run 2: [0,...,49, 250,...,299]
    for (let i = 0; i < 50; i++) ta[100 + i] = i;
    for (let i = 0; i < 50; i++) ta[150 + i] = 250 + i;
    ta.sort((x, y) => x - y);
    for (let i = 1; i < ta.length; i++) {
        if (ta[i] < ta[i - 1])
            throw new Error("typed-array-gallop: not sorted at " + i);
    }
}

// 10c. TypedArray sawtooth.
{
    let ta = new Uint16Array(300);
    for (let seg = 0; seg < 6; seg++) {
        let base = seg * 50;
        if (seg % 2 === 0) {
            for (let i = 0; i < 50; i++) ta[base + i] = base + i;
        } else {
            for (let i = 0; i < 50; i++) ta[base + i] = base + 49 - i;
        }
    }
    ta.sort((x, y) => x - y);
    for (let i = 1; i < ta.length; i++) {
        if (ta[i] < ta[i - 1])
            throw new Error("typed-array-sawtooth: not sorted at " + i);
    }
}

// ============================================================================
// Section 11: Boolean comparator (JSC-specific coercion)
// ============================================================================

// 11a. Boolean comparator with descending input.
// coerceComparatorResultToBoolean returns !result for booleans.
// (a, b) => b <= a: when b <= a, returns true, coerced to false (not less-than).
//                   when b > a, returns false, coerced to true (less-than).
// So this sorts ascending.
{
    let a = [];
    for (let i = 200; i >= 1; i--) a.push(i);
    let sorted = a.slice().sort((x, y) => y <= x);
    assertSorted(sorted, "boolean-cmp-desc-input");
}

// 11b. Boolean comparator with sawtooth input.
{
    let a = [];
    for (let seg = 0; seg < 10; seg++) {
        if (seg % 2 === 0) {
            for (let i = 0; i < 30; i++) a.push(seg * 30 + i);
        } else {
            for (let i = 29; i >= 0; i--) a.push(seg * 30 + i);
        }
    }
    let sorted = a.slice().sort((x, y) => y <= x);
    assertSorted(sorted, "boolean-cmp-sawtooth");
}

// 11c. Boolean comparator stability.
{
    let a = [];
    for (let i = 0; i < 200; i++)
        a.push({ value: i % 10, index: i });
    a.sort((x, y) => y.value <= x.value);
    assertStable(a, "boolean-cmp-stability");
}

// ============================================================================
// Section 12: toSorted (returns new array, tests same code path)
// ============================================================================

// 12a. toSorted with galloping pattern.
{
    let a = [];
    for (let i = 200; i < 300; i++) a.push(i);
    for (let i = 0; i < 50; i++) a.push(i);
    for (let i = 250; i < 300; i++) a.push(i);
    let sorted = a.toSorted(cmp);
    assertSorted(sorted, "toSorted-gallop");
    assertSameElements(a, sorted, "toSorted-gallop");
    // Verify original is unmodified.
    if (a[0] !== 200)
        throw new Error("toSorted modified original");
}

// 12b. toSorted stability.
{
    let a = [];
    for (let i = 0; i < 300; i++)
        a.push({ value: i % 7, index: i });
    let sorted = a.toSorted((x, y) => x.value - y.value);
    assertStable(sorted, "toSorted-stability");
}
