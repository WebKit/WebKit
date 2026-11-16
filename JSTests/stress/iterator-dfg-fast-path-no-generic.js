// Regression coverage for the op_iterator_next DFG fast path when seenModes
// contains exactly FastArray + FastMap (no Generic). Prior to the fix, the
// FastArray branch did not decrement numberOfRemainingModes and used a
// `!= 1` test, so after FastMap decremented and allocated its own failedBlock,
// the chain had a dangling failedBlock with no consumer. Same hazard for
// FastArray + FastSet and FastArray + FastMap + FastSet (no Generic).
//
// Exercises all three combinations through tiered compilation so the DFG
// parser encounters each seenModes shape in practice.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad: ' + actual + ' vs ' + expected);
}
noInline(shouldBe);

function sumAny(iterable) {
    let total = 0;
    for (const entry of iterable) {
        if (typeof entry === "number")
            total += entry;                 // Array / Set
        else
            total += entry[0] + entry[1];   // Map entry
    }
    return total;
}
noInline(sumAny);

const arr = [1, 2, 3, 4, 5];                           // 15
const map = new Map([[1, 10], [2, 20], [3, 30]]);      // 66
const set = new Set([100, 200, 300, 400]);             // 1000

// Case 1: FastArray + FastMap (no Generic, no FastSet).
function runArrayMap() {
    for (let i = 0; i < testLoopCount; ++i) {
        if (i & 1)
            shouldBe(sumAny(arr), 15);
        else
            shouldBe(sumAny(map), 66);
    }
}
runArrayMap();

// Case 2: FastArray + FastSet (no Generic, no FastMap).
function runArraySet() {
    function sumArrOrSet(iter) {
        let total = 0;
        for (const v of iter)
            total += v;
        return total;
    }
    noInline(sumArrOrSet);

    for (let i = 0; i < testLoopCount; ++i) {
        if (i & 1)
            shouldBe(sumArrOrSet(arr), 15);
        else
            shouldBe(sumArrOrSet(set), 1000);
    }
}
runArraySet();

// Case 3: FastArray + FastMap + FastSet, still no Generic.
function runAllFastNoGeneric() {
    for (let i = 0; i < testLoopCount; ++i) {
        const idx = i % 3;
        if (idx === 0)
            shouldBe(sumAny(arr), 15);
        else if (idx === 1)
            shouldBe(sumAny(map), 66);
        else
            shouldBe(sumAny(set), 1000);
    }
}
runAllFastNoGeneric();
