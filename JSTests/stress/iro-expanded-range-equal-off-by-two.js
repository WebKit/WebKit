//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0", "--useOSREntryToDFG=0")

// Regression test for an off-by-2 in IRO's expandedRangeFor when the node's
// only Equal relationship is to a non-constant Primary with a known range.
//
// expandedRangeFor walks each existing relationship and refines a tracked
// [min, max] pair, then writes those bounds back as `lhs < m_zero + max`
// and `lhs > m_zero + min`. Because `lhs > m_zero + V` encodes `lhs >= V+1`,
// the GreaterThan-offset for a desired floor of V must be V-1.
//
// For `lhs == right + offset` (Equal), the floor is `right.minValue + offset`,
// so the offset we write must be `right.minValue + offset - 1`. The buggy
// code wrote `right.minValue + offset + 1`, encoding `lhs >= right.minValue
// + offset + 2` — two above the true floor. Combined with the relationship-
// copying in ArithAdd (the wrong floor is propagated to subsequent
// arithmetic results with the offset shifted along), IRO ended up believing
// values were two larger than they could be, and folded equality checks
// against the real value to FALSE.
//
// Trigger sequence (no arrays involved — pure integer-range reasoning):
//   i = 0                  (entry into a loop)
//   j = (i + 1)|0           Unchecked ArithAdd: sets j == i + 1 (Equal)
//   k = (j + 1)|0           Unchecked ArithAdd: expandedRangeFor(j) fires.
//                           Real range of j on first iter: {1}.
//                           Buggy claim: j >= 3.
//                           Copied to k with +1: k >= 4.
//   if (k === 2) ...        On first iter k = 2. IRO folds the test to FALSE.

function fn() {
    let count = 0;
    let i = 0;
    while (i < 10) {
        let j = (i + 1)|0;
        let k = (j + 1)|0;
        if (k === 2) count = (count + 100)|0;
        i = (i + 1)|0;
    }
    return count;
}
noInline(fn);

for (let r = 0; r < testLoopCount; ++r)
    fn();

const r = fn();
if (r !== 100)
    throw new Error("MISCOMPILE: got " + r + ", expected 100. The "
        + "`k === 2` branch was folded to FALSE; the `count += 100` body "
        + "never executed. This is the expandedRangeFor Equal-min off-by-2 "
        + "bug (see file header).");

print("PASS");
