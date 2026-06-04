//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// IRO can produce Relationships whose (kind, offset) need arithmetic
// outside int32 to lower - Equal at INT32_MIN/MAX (offset+/-1 overflows)
// and GreaterThan at INT32_MIN (-offset overflows). Two known sources:
//   1. filterConstant promoting a more-vague relationship into Equal at
//      the difference of two extreme constants (e.g. INT32_MAX - 0).
//   2. IROFactPoison directly constructing GreaterThan(node, m_zero,
//      minValue - 1) when minValue == INT32_MIN + 1.
// The validator stores AssertInBounds offsets as int64 and does all
// arithmetic in int64, so these encode cleanly without special cases.
// This test drives both sources and verifies FTL compiles + runs them
// correctly. Coverage is non-vacuous as long as each case reaches FTL.

const INT32_MIN = -2147483648;
const INT32_MAX =  2147483647;
const ftlTrue = $vm.ftlTrue;

let didFTLCompile = {
    filterConstantPromotesToEqMax: 0,
    filterConstantPromotesToEqMin: 0,
    iroFactPoisonMinPlusOne: 0,
    iroFactPoisonMaxMinusOne: 0,
};

function assert(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + ", got " + actual);
}

// (1) filterConstant.NotEqual(Equal at int32 limit): establish x ===
// INT32_LIMIT first (records Equal(x, LIMIT_const, 0)), then introduce
// x !== 0. setOneSide refines the NotEqual against the existing Equal
// and would produce Equal(x, 0_const, LIMIT) if not guarded.
function filterConstantPromotesToEqMax(x) {
    didFTLCompile.filterConstantPromotesToEqMax |= ftlTrue();
    if (x === INT32_MAX) {
        if (x !== 0)
            return 1;
        return 2;
    }
    return 0;
}
noInline(filterConstantPromotesToEqMax);

function filterConstantPromotesToEqMin(x) {
    didFTLCompile.filterConstantPromotesToEqMin |= ftlTrue();
    if (x === INT32_MIN) {
        if (x !== 0)
            return 1;
        return 2;
    }
    return 0;
}
noInline(filterConstantPromotesToEqMin);

// (2) IROFactPoison with minValue == INT32_MIN + 1 builds
// GreaterThan(node, m_zero, INT32_MIN) directly; setOneSide must drop it.
// The symmetric maxValue == INT32_MAX - 1 case produces LessThan(node,
// m_zero, INT32_MAX), which is fine to store - kept here so future
// changes that incidentally tighten LessThan also stay tested.
function iroFactPoisonMinPlusOne(x) {
    didFTLCompile.iroFactPoisonMinPlusOne |= ftlTrue();
    return $vm.iroFactPoison(x | 0, -2147483647, 2147483647);
}
noInline(iroFactPoisonMinPlusOne);

function iroFactPoisonMaxMinusOne(x) {
    didFTLCompile.iroFactPoisonMaxMinusOne |= ftlTrue();
    return $vm.iroFactPoison(x | 0, -2147483648, 2147483646);
}
noInline(iroFactPoisonMaxMinusOne);

// Keep both branches of `x === INT32_LIMIT` hot so FTL compiles each
// function with the inner block live (which is where filterConstant is
// asked to refine NotEqual against an existing Equal at the extreme).
for (let i = 0; i < testLoopCount; ++i) {
    filterConstantPromotesToEqMax(INT32_MAX);
    filterConstantPromotesToEqMax(INT32_MAX);
    filterConstantPromotesToEqMax(i & 1);
    filterConstantPromotesToEqMin(INT32_MIN);
    filterConstantPromotesToEqMin(INT32_MIN);
    filterConstantPromotesToEqMin(i & 1);
    iroFactPoisonMinPlusOne(0);
    iroFactPoisonMinPlusOne(-2147483647);
    iroFactPoisonMaxMinusOne(0);
    iroFactPoisonMaxMinusOne(2147483646);
}

assert(filterConstantPromotesToEqMax(INT32_MAX), 1, "filterConstantPromotesToEqMax(INT32_MAX)");
assert(filterConstantPromotesToEqMax(0), 0, "filterConstantPromotesToEqMax(0)");
assert(filterConstantPromotesToEqMin(INT32_MIN), 1, "filterConstantPromotesToEqMin(INT32_MIN)");
assert(filterConstantPromotesToEqMin(0), 0, "filterConstantPromotesToEqMin(0)");
assert(iroFactPoisonMinPlusOne(0), 0, "iroFactPoisonMinPlusOne(0)");
assert(iroFactPoisonMinPlusOne(-2147483647), -2147483647, "iroFactPoisonMinPlusOne(INT32_MIN+1)");
assert(iroFactPoisonMaxMinusOne(0), 0, "iroFactPoisonMaxMinusOne(0)");
assert(iroFactPoisonMaxMinusOne(2147483646), 2147483646, "iroFactPoisonMaxMinusOne(INT32_MAX-1)");

for (const name of Object.keys(didFTLCompile)) {
    if (!didFTLCompile[name])
        throw new Error(name + " never reached FTL - setOneSide extreme-offset rejection untested for this path");
}
