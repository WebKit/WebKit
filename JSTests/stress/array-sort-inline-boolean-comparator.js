// The DFG ArraySortIntrinsic's inline insertion sort must interpret the comparator
// result the same way runtime/StableSort.h's coerceComparatorResultToBoolean does:
//
//   - Int32: shift iff `< 0`.
//   - Boolean: shift iff `!result` (true->no shift, false->shift) -- webkit.org/b/47825.
//   - Other: shift iff `toNumber(result) < 0` (NaN coerces to 0 in V8 but in JSC's
//     path `NaN < 0` is false, matching StableSort.h's `toNumber() < 0` for NaN).
//
// Before the fix the intrinsic did only `cmp < 0`, which gave false for boolean
// `false` (since 0 < 0 is false), so a `() => false` comparator was a no-op and
// the legacy `(a, b) => a > b` pattern never sorted. The fix routes `cmp === false`
// through an extra branch that also goes to the shift target.

function check(got, expected) {
    if (got.length !== expected.length)
        throw new Error("length mismatch: " + JSON.stringify(got) + " vs " + JSON.stringify(expected));
    for (let i = 0; i < expected.length; ++i) {
        if (!Object.is(got[i], expected[i]))
            throw new Error("index " + i + " mismatch: " + JSON.stringify(got) + " vs " + JSON.stringify(expected));
    }
}

function sortIt(a, c) { return a.sort(c); }

// Warm up with a well-typed Int32 comparator so DFG inlines the intrinsic on the
// hot doSort path.
for (let i = 0; i < testLoopCount; ++i)
    sortIt([5, 3, 1, 4, 2], (a, b) => a - b);

// Expected results are computed via the generic baseline sort on a fresh array
// (no DFG intrinsic cached state). Matches runtime/StableSort.h exactly.
function expected(input, cmp) { return [...input].sort(cmp); }

// --- Case A: Int32 comparator (primary fast path). ----------------------------
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), (a, b) => a - b), [1, 2, 3, 4, 5]);
    check(sortIt(input.slice(), (a, b) => b - a), [5, 4, 3, 2, 1]);
}

// Int32 boundary: zero / MIN_VALUE / MAX_VALUE / negative / positive.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    const cmp = (a, b) => {
        const d = a - b;
        if (d === 0) return 0;
        return d < 0 ? -2147483648 : 2147483647;  // extreme Int32s
    };
    check(sortIt(input.slice(), cmp), expected(input, cmp));
}

// --- Case B: Boolean comparator (legacy b/47825 path). -----------------------
// Constant-false: every comparison is "shift", insertion sort pushes each pivot
// to the front.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), () => false), expected(input, () => false));
}

// Constant-true: every comparison is "no shift", array stays as-is.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), () => true), expected(input, () => true));
}

// The real-world legacy pattern: comparator returns a boolean. Both these must
// end up ascending / descending respectively, matching baseline.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), (a, b) => a > b), [1, 2, 3, 4, 5]);
    check(sortIt(input.slice(), (a, b) => a < b), [5, 4, 3, 2, 1]);
}

// Mixed boolean / int return in a single comparator -- exercises both the
// CompareLess branch (shift when < 0) and the CompareStrictEq branch (shift when
// boolean false) in the same sort.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [7, 2, 5, 3, 8, 1, 6, 4, 0, 9];
    const cmp = (a, b) => {
        if ((a ^ b) & 1) return a > b;  // boolean
        return a - b;                   // int32
    };
    check(sortIt(input.slice(), cmp), expected(input, cmp));
}

// --- Case C: Double comparator. --------------------------------------------
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    const cmp = (a, b) => (a - b) + 0.5 - 0.5;  // Double result.
    check(sortIt(input.slice(), cmp), expected(input, cmp));
}

// Double edge cases: -0, +0, NaN, Infinity.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    const table = new Map([
        ['lt', -Infinity],
        ['gt', Infinity],
        ['eq', 0],
        ['negZero', -0],  // -0 < 0 is false per IEEE 754
        ['nan', NaN],     // NaN < 0 is false
    ]);
    for (const [, v] of table) {
        const cmp = (a, b) => a === b ? 0 : (a < b ? v : -v);
        check(sortIt(input.slice(), cmp), expected(input, cmp));
    }
}

// --- Case D: String comparator. Falls back to toNumber() < 0 path. -----------
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    const cmp = (a, b) => a === b ? "0" : (a < b ? "-1" : "1");
    check(sortIt(input.slice(), cmp), expected(input, cmp));
}

// String coerces to NaN → NaN < 0 → false → no shift.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), () => "abc"), expected(input, () => "abc"));
}

// --- Case E: null / undefined. ------------------------------------------------
// toNumber(null) = 0 → 0 < 0 = false → no shift.
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    check(sortIt(input.slice(), () => null), expected(input, () => null));
    check(sortIt(input.slice(), () => undefined), expected(input, () => undefined));
}

// --- Case F: Object with valueOf. toPrimitive then toNumber. ------------------
for (let i = 0; i < testLoopCount; ++i) {
    const input = [5, 3, 1, 4, 2];
    const neg = { valueOf() { return -1; } };
    const pos = { valueOf() { return 1; } };
    const zero = { valueOf() { return 0; } };
    // Return objects -- each sort iteration triggers valueOf.
    check(sortIt(input.slice(), (a, b) => a < b ? neg : a > b ? pos : zero), [1, 2, 3, 4, 5]);
    check(sortIt(input.slice(), () => neg), expected(input, () => neg));
}

// --- Case G: BigInt comparator. toNumber throws TypeError. -------------------
// Both baseline and intrinsic must throw.
for (let i = 0; i < testLoopCount; ++i) {
    let threw = false;
    try { sortIt([5, 3, 1], () => -1n); }
    catch (e) { threw = e instanceof TypeError; }
    if (!threw) throw new Error("BigInt comparator must throw TypeError");
}
