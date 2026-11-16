// Verifies that a naturally-occurring OSR exit inside the DFG-inlined comparator
// (via Int32 speculation failure on ValueSub) recovers correctly and the sort
// call completes with the right result. The existing
// array-sort-inline-osr-exit-in-comparator.js uses `$vm.OSRExit()` to force an
// exit synthetically; this test exercises the same code path that a real-world
// caller would hit: warm up with Int32 inputs so DFG speculates Int32, then
// feed doubles and watch the speculation fail mid-sort.
//
// The exit lands inside the comparator's inlined body. Per
// InlineCallFrame::Kind::ArraySortComparatorCall, the comparator's baseline
// resumes, completes, and returns via arraySortComparatorReturnTrampoline --
// which discards the return value and re-dispatches the caller's op_call.
// Baseline then calls sort generically with the correctly-preserved arguments.
// The observable result must still be a correctly sorted array.

function cmp(a, b) { return a - b; }

function sortIt(a, c) { return a.sort(c); }

function assertSorted(a, reference, label) {
    if (a.length !== reference.length)
        throw new Error(label + ": length " + a.length + " vs " + reference.length);
    for (let i = 0; i < reference.length; ++i) {
        if (a[i] !== reference[i])
            throw new Error(label + ": @" + i + " got " + a[i] + " want " + reference[i]);
    }
}

// --- Phase 1: warm up with Int32 arrays so DFG inlines sort + inlines the
// comparator. DFG/FTL will profile `cmp`'s ValueSub as Int32Use.
const int32Seed = [5, 3, 1, 4, 2, 8, 7, 6, 0, 9, 11, 14, 13, 10, 12, 15];
const int32Expected = int32Seed.slice().sort(cmp);
for (let w = 0; w < testLoopCount; ++w)
    assertSorted(sortIt(int32Seed.slice(), cmp), int32Expected, "warmup int32 " + w);

// --- Phase 2: now feed doubles. The comparator's Int32-speculated ValueSub
// sees double operands and OSR-exits inside the comparator body. Baseline
// finishes the comparator and jumps through the trampoline back to op_call,
// which re-runs sort generically. Each iteration must still produce a
// correctly sorted array.
const doubleSeed = [5.5, 3.25, 1.75, 4.5, 2.125, 8.8, 7.1, 6.6, 0.5, 9.9, 11.1, 14.4, 13.3, 10.0, 12.2, 15.5];
const doubleExpected = doubleSeed.slice().sort(cmp);
for (let w = 0; w < testLoopCount; ++w)
    assertSorted(sortIt(doubleSeed.slice(), cmp), doubleExpected, "double " + w);

// --- Phase 3: alternate back to Int32. The call site has tiered back up by
// now; must still sort correctly.
for (let w = 0; w < testLoopCount; ++w)
    assertSorted(sortIt(int32Seed.slice(), cmp), int32Expected, "back-to-int " + w);

// --- Phase 4: mix Int32 and Double in the same array. Each iteration may
// exit mid-sort as the comparator hits a double.
const mixedSeed = [5, 3.25, 1, 4.5, 2, 8, 7.1, 0, 6.6, 9];
const mixedExpected = mixedSeed.slice().sort(cmp);
for (let w = 0; w < testLoopCount; ++w)
    assertSorted(sortIt(mixedSeed.slice(), cmp), mixedExpected, "mixed " + w);

// --- Phase 5: arrays of varying size around the 16-element boundary, each
// with doubles, so the exit fires on both the insertion-sort fast path and
// (for len > 16) the generic slow path. The array length 1 case is boring
// (no comparator call) so start at 2.
for (let len = 2; len <= 20; ++len) {
    const seed = [];
    for (let i = 0; i < len; ++i) seed.push((len - i) + 0.5);
    const expected = seed.slice().sort(cmp);
    for (let w = 0; w < 100; ++w)
        assertSorted(sortIt(seed.slice(), cmp), expected, "len " + len + " w " + w);
}

// --- Phase 6: comparator that throws inside the inlined body. If OSR exit
// handling is wrong, the exception might propagate from the wrong frame or
// the sort call may appear to return a value (and spread would then crash,
// which is the WebGPU bug we started from). Here we just assert the thrown
// error is observed intact at the caller.
let shouldThrow = false;
function throwingCmp(a, b) {
    if (shouldThrow)
        throw new Error("boom");
    return a - b;
}
// Warm up so the comparator is inlined.
for (let w = 0; w < testLoopCount; ++w) sortIt([3, 1, 2], throwingCmp);

shouldThrow = true;
let threw = false;
try { sortIt([5, 3, 1, 4, 2], throwingCmp); }
catch (e) { threw = e.message === "boom"; }
if (!threw)
    throw new Error("throwing comparator did not propagate through the sort");
