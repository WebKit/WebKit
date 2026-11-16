// Stress the DFG iterator_next fast path for Map iteration under "having a bad time".
// In that state, the NewArray([key, value]) pair-allocation for Map entries takes
// the operationNewArray slow path (a C++ call under DECLARE_THROW_SCOPE). This
// exercises NewArray's ExitsForExceptions classification: if allocation throws,
// exception unwind must not replay the already-advanced MapIteratorNext.
//
// Two phases:
//   (1) warm up with normal allocation so DFG/FTL compiles the fast NewArray path,
//   (2) flip to bad-time (invalidates the havingABadTime watchpoint installed by
//       FixupPhase::watchHavingABadTime on NewArray) to force recompilation through
//       the slow allocation path.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad: ' + actual + ' vs ' + expected);
}
noInline(shouldBe);

const map = new Map();
for (let i = 0; i < 16; ++i)
    map.set(i, i * 10);
// 0+0 + 1+10 + ... + 15+150 = sum(0..15) + 10*sum(0..15) = 120 + 1200 = 1320
const expectedMapSum = 1320;

const set = new Set();
for (let i = 0; i < 16; ++i)
    set.add(i);
const expectedSetSum = 120;

function sumMapEntries(m) {
    let total = 0;
    for (const [k, v] of m)
        total += k + v;
    return total;
}
noInline(sumMapEntries);

function sumSetValues(s) {
    let total = 0;
    for (const v of s)
        total += v;
    return total;
}
noInline(sumSetValues);

// Phase 1: warm up with normal fast allocation.
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sumMapEntries(map), expectedMapSum);
    shouldBe(sumSetValues(set), expectedSetSum);
}

// Phase 2: trigger bad-time by installing a getter on a numeric-indexed slot
// of Object.prototype. This fires the havingABadTime watchpoint, invalidating
// any compiled code that relied on NewArray taking the fast inline-allocation
// path. Subsequent runs must still iterate correctly.
Object.defineProperty(Object.prototype, "0", {
    get() { return "shadow"; },
    set(v) { },
    configurable: true,
});

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(sumMapEntries(map), expectedMapSum);
    shouldBe(sumSetValues(set), expectedSetSum);
}
