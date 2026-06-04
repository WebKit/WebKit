//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// IRO must never produce a Relationship whose encoded offset reshapes outside
// int32 when fed to AssertInBounds - e.g. GreaterThan with offset == INT32_MIN
// (would need -offset = INT32_MAX + 1), or Equal with offset == INT32_MAX or
// INT32_MIN (would need offset +/- 1 to fit). Relationship's construction paths
// (flipped, inverse, addToOffset, mergeConstant) use sumOverflows to bail on
// these. The validator's insertSingleRelationshipAssertion RELEASE_ASSERTs via
// fitsInt32() on every derived offset, so any IRO leak crashes here.
//
// This test drives comparisons and arithmetic against the int32 limits through
// FTL with the validator enabled. If any pattern coaxes IRO into a boundary
// relationship, the validator crashes; if no invariant breaks, results match
// the expected values.

const ftlTrue = $vm.ftlTrue;
// Pre-seed every key with Int32 0, and use `|= ftlTrue()` rather than the
// `if (ftlTrue()) ... = true` branch form. Branching on ftlTrue() gives DFG a
// profile where the true arm is never taken (DFG sees ftlTrue()===0), so the
// FTL version OSR-exits with InadequateCoverage the moment the true arm
// executes and the AssertInBounds path is never actually run. The
// bitwise-OR form keeps FTL resident through the loop and avoids the
// object-Structure transition that would invalidate sibling FTL-compiled
// callees.
let didFTLCompile = {
    eqMax: 0, eqMin: 0,
    ltMax: 0, gtMin: 0, ltMin: 0, gtMax: 0,
    neMax: 0, neMin: 0,
    subNearMax: 0, subNearMin: 0,
    loopDownFromMax: 0,
    twoSidedAtMax: 0, twoSidedAtMin: 0,
};

function assert(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + ", got " + actual);
}

const INT32_MIN = -2147483648;
const INT32_MAX =  2147483647;

// ============================================================================
// (1) Direct equality with each int32 limit. Forms Relationship(x, m_zero,
//     Equal, INT32_MAX) / (..., INT32_MIN). If IRO recorded these, the
//     validator would try fitsInt32(INT32_MAX + 1) -> RELEASE_ASSERT.
// ============================================================================
function eqMax(x) {
    didFTLCompile.eqMax |= ftlTrue();
    if (x === INT32_MAX) return 1;
    return 0;
}
noInline(eqMax);

function eqMin(x) {
    didFTLCompile.eqMin |= ftlTrue();
    if (x === INT32_MIN) return 1;
    return 0;
}
noInline(eqMin);

// ============================================================================
// (2) Strict inequality with each limit. LessThan with offset == INT32_MIN or
//     GreaterThan with offset == INT32_MAX would be unsatisfiable; LessThan
//     with offset == INT32_MAX or GreaterThan with offset == INT32_MIN are
//     boundary-but-trivial. IRO's inverse() path (LessThan->GreaterThan and
//     vice-versa) bails on these via sumOverflows.
// ============================================================================
function ltMax(x) {
    didFTLCompile.ltMax |= ftlTrue();
    if (x < INT32_MAX) return 1;
    return 0;
}
noInline(ltMax);

function gtMin(x) {
    didFTLCompile.gtMin |= ftlTrue();
    if (x > INT32_MIN) return 1;
    return 0;
}
noInline(gtMin);

function ltMin(x) {
    didFTLCompile.ltMin |= ftlTrue();
    if (x < INT32_MIN) return 1;       // unsatisfiable for int32 x
    return 0;
}
noInline(ltMin);

function gtMax(x) {
    didFTLCompile.gtMax |= ftlTrue();
    if (x > INT32_MAX) return 1;       // unsatisfiable for int32 x
    return 0;
}
noInline(gtMax);

// ============================================================================
// (3) NotEqual at each limit. Encodes as a single AssertInBounds with offset
//     == INT32_MIN/MAX; fitsInt32(offset) passes (int32-stored) but the
//     reshape into NotEqual must not derive offset+/-1.
// ============================================================================
function neMax(x) {
    didFTLCompile.neMax |= ftlTrue();
    if (x !== INT32_MAX) return 1;
    return 0;
}
noInline(neMax);

function neMin(x) {
    didFTLCompile.neMin |= ftlTrue();
    if (x !== INT32_MIN) return 1;
    return 0;
}
noInline(neMin);

// ============================================================================
// (4) Comparisons after a checked subtraction by a constant near the limit.
//     ArithSub here is overflow-checked, so the result is int32; IRO will
//     try to relate `(x - k)` to its operands with a non-trivial offset that
//     could pile up near the boundary if it weren't carefully bounded.
// ============================================================================
function subNearMax(x) {
    didFTLCompile.subNearMax |= ftlTrue();
    let d = x - (INT32_MAX - 3);
    if (d >= 0 && d < 8) return d;
    return -1;
}
noInline(subNearMax);

function subNearMin(x) {
    didFTLCompile.subNearMin |= ftlTrue();
    let d = x - (INT32_MIN + 3);
    if (d >= 0 && d < 8) return d;
    return -1;
}
noInline(subNearMin);

// ============================================================================
// (5) Loop counting down from a value near INT32_MAX. The loop induction
//     variable accumulates IRO facts each round; if any incremental
//     setEquivalence/addToOffset crossed the int32 boundary, the validator
//     would catch it on the next iteration.
// ============================================================================
function loopDownFromMax(start, count) {
    didFTLCompile.loopDownFromMax |= ftlTrue();
    let i = start;
    let total = 0;
    for (let k = 0; k < count; ++k) {
        if (i > INT32_MIN + 8)
            total += 1;
        i -= 1;
    }
    return total;
}
noInline(loopDownFromMax);

// ============================================================================
// (6) Two-sided range guard at the int32 limits - forms both a LessThan and
//     GreaterThan fact against the same node, exercising the Equal-expansion
//     path that derives both offset+1 and -offset+1 from rel.offset().
// ============================================================================
function twoSidedAtMax(x) {
    didFTLCompile.twoSidedAtMax |= ftlTrue();
    if (x > INT32_MAX - 4 && x < INT32_MAX)
        return x - (INT32_MAX - 4);
    return -1;
}
noInline(twoSidedAtMax);

function twoSidedAtMin(x) {
    didFTLCompile.twoSidedAtMin |= ftlTrue();
    if (x > INT32_MIN && x < INT32_MIN + 4)
        return x - INT32_MIN;
    return -1;
}
noInline(twoSidedAtMin);

for (let i = 0; i < testLoopCount; ++i) {
    eqMax(i | 0);
    eqMax(INT32_MAX);
    eqMin(i | 0);
    eqMin(INT32_MIN);

    ltMax(INT32_MAX - 1);
    ltMax(INT32_MAX);
    gtMin(INT32_MIN + 1);
    gtMin(INT32_MIN);
    ltMin(0);
    ltMin(INT32_MIN);
    gtMax(0);
    gtMax(INT32_MAX);

    neMax(INT32_MAX);
    neMax(0);
    neMin(INT32_MIN);
    neMin(0);

    subNearMax(INT32_MAX - 2);
    subNearMax(0);
    subNearMin(INT32_MIN + 2);
    subNearMin(0);

    loopDownFromMax(INT32_MAX, 4);
    loopDownFromMax(INT32_MIN + 16, 4);

    twoSidedAtMax(INT32_MAX - 2);
    twoSidedAtMax(0);
    twoSidedAtMin(INT32_MIN + 2);
    twoSidedAtMin(0);
}

assert(eqMax(INT32_MAX), 1, "eqMax(INT32_MAX)");
assert(eqMax(0), 0, "eqMax(0)");
assert(eqMin(INT32_MIN), 1, "eqMin(INT32_MIN)");
assert(eqMin(0), 0, "eqMin(0)");

assert(ltMax(INT32_MAX), 0, "ltMax(INT32_MAX)");
assert(ltMax(0), 1, "ltMax(0)");
assert(gtMin(INT32_MIN), 0, "gtMin(INT32_MIN)");
assert(gtMin(0), 1, "gtMin(0)");
assert(ltMin(INT32_MIN), 0, "ltMin(INT32_MIN)");
assert(ltMin(0), 0, "ltMin(0)");
assert(gtMax(INT32_MAX), 0, "gtMax(INT32_MAX)");
assert(gtMax(0), 0, "gtMax(0)");

assert(neMax(INT32_MAX), 0, "neMax(INT32_MAX)");
assert(neMax(0), 1, "neMax(0)");
assert(neMin(INT32_MIN), 0, "neMin(INT32_MIN)");
assert(neMin(0), 1, "neMin(0)");

assert(subNearMax(INT32_MAX - 3), 0, "subNearMax low");
assert(subNearMax(INT32_MAX), 3, "subNearMax high in range");
assert(subNearMax(0), -1, "subNearMax under range");
assert(subNearMin(INT32_MIN + 3), 0, "subNearMin low in range");
assert(subNearMin(INT32_MIN), -1, "subNearMin under range");
assert(subNearMin(0), -1, "subNearMin over range");

assert(twoSidedAtMax(INT32_MAX - 2), 2, "twoSidedAtMax inside");
assert(twoSidedAtMax(INT32_MAX), -1, "twoSidedAtMax outside high");
assert(twoSidedAtMax(0), -1, "twoSidedAtMax outside low");
assert(twoSidedAtMin(INT32_MIN + 2), 2, "twoSidedAtMin inside");
assert(twoSidedAtMin(INT32_MIN), -1, "twoSidedAtMin outside low");
assert(twoSidedAtMin(0), -1, "twoSidedAtMin outside high");

for (const name of ["eqMax", "eqMin", "ltMax", "gtMin", "ltMin", "gtMax",
                    "neMax", "neMin", "subNearMax", "subNearMin",
                    "loopDownFromMax", "twoSidedAtMax", "twoSidedAtMin"]) {
    if (!didFTLCompile[name])
        throw new Error(name + " never reached FTL - int32 boundary relationship path untested for this case");
}
