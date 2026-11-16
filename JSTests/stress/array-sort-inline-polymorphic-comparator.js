// The DFG ArraySortIntrinsic's comparator shape detection (tryInlineComparator)
// checks for a JSFunction constant or a same-graph NewFunction allocation. A
// sort call site that sees multiple distinct comparator functions fails both
// checks and falls through to a plain Call node at the comparator call site.
//
// This test rotates several comparator functions at a single sort call site:
//   * The DFG cannot statically resolve the comparator -> uses the plain-Call
//     path (addCallWithoutSettingResult) instead of handleCall inlining.
//   * The IsCallable runtime guard must fire correctly whether or not the
//     comparator is the same as the most recent one.
//   * Sort results must remain correct across all comparator shapes.

function cmpAsc(a, b) { return a - b; }
function cmpDesc(a, b) { return b - a; }
function cmpMod3Asc(a, b) { return (a % 3) - (b % 3); }
function cmpByAbs(a, b) { return Math.abs(a) - Math.abs(b); }

const comparators = [cmpAsc, cmpDesc, cmpMod3Asc, cmpByAbs];

function doSort(a, c) { return a.sort(c); }

function expectedFor(input, cmp) { return [...input].sort(cmp); }

// Warm up with a rotating comparator so the call site is polymorphic from the
// start -- DFG/FTL never see a single callee at this site.
const seed = [5, -3, 1, -4, 2, 8, -7, 0, 6, -1];
for (let w = 0; w < testLoopCount; ++w) {
    const cmp = comparators[w % comparators.length];
    const got = doSort(seed.slice(), cmp);
    const want = expectedFor(seed, cmp);
    if (got.length !== want.length)
        throw new Error("warmup " + w + ": length mismatch");
    for (let i = 0; i < want.length; ++i) {
        if (got[i] !== want[i])
            throw new Error("warmup " + w + " cmp=" + cmp.name
                + " index " + i + ": got " + got[i] + " want " + want[i]);
    }
}

// Non-callable comparators must still throw TypeError from this polymorphic
// call site (IsCallable guard must not be skipped for non-provably-callable
// comparators).
for (let w = 0; w < 100; ++w) {
    let threw = false;
    try { doSort(seed.slice(), {}); } catch (e) { threw = e instanceof TypeError; }
    if (!threw) throw new Error("polymorphic site: {} must throw TypeError");
}

// A comparator that throws from the body must propagate correctly even when
// the call site is polymorphic (plain-Call path, no inlining).
function cmpThrow(a, b) { throw new Error("boom"); }
let threw = false;
try { doSort([3, 1], cmpThrow); }
catch (e) { threw = e.message === "boom"; }
if (!threw) throw new Error("polymorphic site: throwing comparator lost the exception");

// Large array + polymorphic comparator (slow-path dispatcher + plain Call).
const bigSeed = [];
for (let i = 0; i < 32; ++i) bigSeed.push(((i * 17) % 32) - 16);
for (let w = 0; w < testLoopCount; ++w) {
    const cmp = comparators[w % comparators.length];
    const got = doSort(bigSeed.slice(), cmp);
    const want = expectedFor(bigSeed, cmp);
    for (let i = 0; i < want.length; ++i) {
        if (got[i] !== want[i])
            throw new Error("large poly w=" + w + " cmp=" + cmp.name + " @" + i);
    }
}
