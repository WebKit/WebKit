// Array.prototype.sort(comparator) must throw TypeError when the comparator
// returns a BigInt (or Symbol), because runtime/StableSort.h's
// coerceComparatorResultToBoolean finishes with `toNumber(cmp) < 0`, and
// ToNumber on a BigInt/Symbol throws TypeError.
//
// Before the fix, the DFG ArraySortIntrinsic modelled the coercion as
// `CompareLess(cmp, 0) || cmp === false`. CompareLess's generic runtime path
// (jsLess) handles BigInt-vs-Number via bigIntCompare without calling ToNumber,
// so if CompareLess ever ran in the inlined fast path with a BigInt input (for
// instance via UntypedUse specialization from polymorphic warmup), the sort
// would silently succeed where baseline throws. The fix replaces the numeric
// branch with an explicit ToNumber node, which throws for BigInt/Symbol and
// brings the DFG intrinsic in line with baseline regardless of speculation
// outcome.

function sortIt(a, c) { return a.sort(c); }

function expectTypeError(body, desc) {
    try { body(); }
    catch (e) {
        if (!(e instanceof TypeError))
            throw new Error(desc + ": expected TypeError, got " + e);
        return;
    }
    throw new Error(desc + ": expected TypeError, got no throw");
}

// --- Case 1: BigInt-only warmup. Drives whatever speculation path the DFG
// picks when it has only seen BigInt returns.
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => -1n), "BigInt-only warmup iter " + i);

// --- Case 2: Int32 warmup, then a BigInt-returning comparator. The Int32
// speculation in the inlined CompareLess/ToNumber chain will fail on a BigInt
// input, but the slow path / baseline must still throw.
for (let i = 0; i < testLoopCount; ++i)
    sortIt([5, 3, 1, 4, 2], (a, b) => a - b);
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => -1n), "after Int32 warmup iter " + i);

// --- Case 3: Mixed warmup (Int32, Boolean, Object) to push CompareLess toward
// UntypedUse. This is the scenario the old code was most vulnerable in: the
// old CompareLess-on-BigInt would NOT throw (returns true via bigIntCompare),
// so the fix's ToNumber is what keeps us correct here.
for (let i = 0; i < testLoopCount; ++i) {
    sortIt([5, 3, 1, 4, 2], (a, b) => a - b);
    sortIt([5, 3, 1, 4, 2], (a, b) => a > b);        // boolean
    sortIt([5, 3, 1, 4, 2], (a, b) => ({ valueOf() { return a - b; } })); // object
}
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => -1n), "after polymorphic warmup iter " + i);

// --- Case 4: Positive BigInt. `toNumber(1n)` also throws, so this must throw too.
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => 1n), "positive BigInt iter " + i);

// --- Case 5: 0n (zero BigInt). `toNumber(0n)` also throws.
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => 0n), "zero BigInt iter " + i);

// --- Case 6: Symbol comparator return. `toNumber(Symbol)` throws TypeError.
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => Symbol.iterator), "Symbol iter " + i);

// --- Case 7: Object whose valueOf returns a BigInt. ToPrimitive-then-ToNumber
// still throws.
for (let i = 0; i < testLoopCount; ++i) {
    const obj = { valueOf() { return -1n; } };
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], () => obj), "object-valueOf-BigInt iter " + i);
}

// --- Case 8: Early-BigInt in an otherwise-numeric comparator. The comparator
// returns Int32 for most pairs but BigInt for one specific (pivot, current)
// pair that actually occurs during insertion sort of [5, 3, 1, 4, 2] --
// the last comparison is cmp(2, 1) when placing pivot=2. Sorting must throw
// once it hits the BigInt case.
for (let i = 0; i < testLoopCount; ++i) {
    const cmp = (a, b) => (a === 2 && b === 1) ? -1n : (a - b);
    expectTypeError(() => sortIt([5, 3, 1, 4, 2], cmp), "mid-sort BigInt iter " + i);
}

// --- Case 9: length-2 array (tiny fast path). Still must throw.
for (let i = 0; i < testLoopCount; ++i)
    expectTypeError(() => sortIt([2, 1], () => -1n), "len2 BigInt iter " + i);

// --- Sanity: non-BigInt comparators must NOT throw, to catch any accidental
// over-throwing regression in the ToNumber path.
for (let i = 0; i < testLoopCount; ++i) {
    const r = sortIt([5, 3, 1, 4, 2], (a, b) => a - b);
    if (r[0] !== 1 || r[4] !== 5) throw new Error("regression in numeric sort: " + r);
}
for (let i = 0; i < testLoopCount; ++i) {
    // null / undefined / string: must not throw.
    sortIt([5, 3, 1, 4, 2], () => null);
    sortIt([5, 3, 1, 4, 2], () => undefined);
    sortIt([5, 3, 1, 4, 2], () => "abc");
}
