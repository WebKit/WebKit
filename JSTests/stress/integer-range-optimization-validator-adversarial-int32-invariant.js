//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Adversarial: try every way I can think of to land a non-Int32 value on an
// AssertInBounds edge. IRO and its validator are Int32-only at every
// consumption site - see the RELEASE_ASSERT at the entry of rangeFor() and the
// hasDoubleResult()/hasInt52Result() skips in insertSingleRelationshipAssertion
// and the inner insertOffset/Upper/LowerBoundAssertion helpers. If any of those
// trip, this test crashes; if no invariant breaks, it returns correct results.

const ftlTrue = $vm.ftlTrue;
// Pre-seed every key with Int32 0, and use `|= ftlTrue()` rather than the
// `if (ftlTrue()) ... = true` branch form. The branch form gives DFG a profile
// where the true arm is never taken (DFG sees ftlTrue()===0), so the FTL
// version OSR-exits with InadequateCoverage the moment the true arm executes
// and the AssertInBounds path is never actually run. The bitwise-OR form
// avoids both the cold-arm OSR exit and the object-Structure transition that
// would otherwise invalidate sibling FTL-compiled callees.
let didFTLCompile = {
    uint32Direct: 0,
    phiIntDouble: 0,
    toLengthOnUint32: 0,
    truncatedArith: 0,
    poisonTruncatedU32: 0,
    uint32AsIndex: 0,
    divThenTrunc: 0,
    nested: 0,
};

function assert(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + ", got " + actual);
}

// ============================================================================
// (1) Uint32Array values exceeding INT32_MAX produce an Int52-result (with
//     enableInt52) or Double-result GetByVal. IRO records `result > -1` against
//     that node. The fact is dead in IRO (no consumer can query it), and the
//     comprehensive validator must skip emitting an AssertInBounds for it.
// ============================================================================
const u32 = new Uint32Array(8);
for (let i = 0; i < u32.length; ++i) u32[i] = 0xFFFFFFF0 + i;

function uint32Direct(arr, i) {
    didFTLCompile.uint32Direct |= ftlTrue();
    let v = arr[i & 7];
    return v + 1;            // ArithAdd not Int32Use here; IRO does not touch
}
noInline(uint32Direct);

// ============================================================================
// (2) Phi merging Int32 and Double in a tight loop. The Phi is Double-result;
//     IRO's setEquivalence in Upsilon/Phi places facts on the Phi's projections.
//     The validator must skip those (Shadow drop + Double-result skip).
// ============================================================================
function phiIntDouble(n, useDouble) {
    didFTLCompile.phiIntDouble |= ftlTrue();
    let x = 0;
    for (let i = 0; i < n; ++i)
        x = useDouble ? x + 0.5 : x + 1;
    return x;
}
noInline(phiIntDouble);

// ============================================================================
// (3) ToLength's executeNode calls provablyNonNegative(child) on an arbitrary
//     useKind (only != UntypedUse). When the child is Double with a `>-1` fact
//     (e.g. from Uint32Array), ToLength adds `result_int32 == child_double` -
//     a mixed-type relationship the validator must skip on the RHS check.
// ============================================================================
function toLengthOnUint32(arr, i) {
    didFTLCompile.toLengthOnUint32 |= ftlTrue();
    let v = arr[i & 7];
    // Pass v as the length of an array-like; slice invokes ToLength on .length
    // unconditionally (step 2 of Array.prototype.slice). The explicit `(0, 0)`
    // bounds skip the materialization loop so the call doesn't try to allocate
    // a 4-billion-element result when v exceeds INT32_MAX.
    return Array.prototype.slice.call({length: v}, 0, 0).length;
}
noInline(toLengthOnUint32);

// ============================================================================
// (4) Truncate a non-Int32 value to Int32 with `| 0`, then run an IRO-
//     optimizable ArithAdd. The truncation IS Int32-result; the ArithAdd is
//     IRO-optimizable; facts come from the BitOr (Int32 LHS) - never from the
//     original Int52/Double producer. The chain reaches AssertInBounds via
//     the per-site insertRangeAssertion at the ArithAdd.
// ============================================================================
function truncatedArith(arr, i) {
    didFTLCompile.truncatedArith |= ftlTrue();
    let v = arr[i & 7];
    let t = v | 0;
    return t + 1;
}
noInline(truncatedArith);

// ============================================================================
// (5) iroFactPoison on a truncated Uint32 value. Tests the IROFactPoison ->
//     Int32Use fixup + the per-site insertFactPoisonRangeAssertion. The claim
//     is on the truncation, not the original.
//     For arr[0..7] = 0xFFFFFFF0..0xFFFFFFF7, `| 0` yields -16..-9.
// ============================================================================
function poisonTruncatedU32(arr, i) {
    didFTLCompile.poisonTruncatedU32 |= ftlTrue();
    let t = arr[i & 7] | 0;
    return $vm.iroFactPoison(t, -16, -9);
}
noInline(poisonTruncatedU32);

// ============================================================================
// (6) Uint32 value flowing into CheckInBounds. The index requires Int32 at
//     the bounds check, so a conversion node sits between the Double producer
//     and CheckInBounds. The CheckInBounds children are Int32 - the per-site
//     var-var assertion must fire only on Int32-result endpoints.
//
// u32Mixed contains both small valid indices (slots 0..3) and values that
// exceed INT32_MAX (slots 4..7). The GetByVal observes both and predicts
// Double, so v is Double-result; data[v] requires Int32 so a Double->Int32
// conversion is inserted before CheckInBounds. With small v in [0, 64) hot,
// the data[v] block survives profile-driven cold-block elimination and IRO
// proves `v < data.length` along that path, triggering the var-var
// assertion insertion at the CheckInBounds.
// ============================================================================
const dataArr = new Array(64);
for (let i = 0; i < dataArr.length; ++i) dataArr[i] = i;

const u32Mixed = new Uint32Array(8);
for (let i = 0; i < 4; ++i) u32Mixed[i] = i;
for (let i = 4; i < 8; ++i) u32Mixed[i] = 0xFFFFFFF0 + i;

function uint32AsIndex(uintArr, data, i) {
    didFTLCompile.uint32AsIndex |= ftlTrue();
    let v = uintArr[i & 7];
    if (v >= 0 && v < data.length)
        return data[v];
    return -1;
}
noInline(uint32AsIndex);

// ============================================================================
// (7) Double producer (ArithDiv) feeding into Int32-typed arithmetic via
//     truncation. Confirms the validator does not pull facts from the Div node.
// ============================================================================
function divThenTrunc(n, k) {
    didFTLCompile.divThenTrunc |= ftlTrue();
    let q = n / k;           // ArithDiv, Double-result
    let t = q | 0;           // Int32-result
    return t + 1;            // IRO-optimizable
}
noInline(divThenTrunc);

// ============================================================================
// (8) Direct test of comprehensive validator at non-trivial CFG: nested loops
//     with conditional branches that add many relationships into m_relationships.
//     Each iteration explores every fact path; the comprehensive walker emits
//     assertions at every exitOK node in every block.
// ============================================================================
function nested(arr, n) {
    didFTLCompile.nested |= ftlTrue();
    let total = 0;
    for (let i = 0; i < n; ++i) {
        if (i >= 0 && i < arr.length) {
            for (let j = 0; j < n; ++j) {
                if (j > i)
                    total += arr[j] - arr[i];
            }
        }
    }
    return total;
}
noInline(nested);

// Drive enough iterations to trigger FTL compilation of every function. Each
// function only needs warmup to reach FTL; extra iterations re-run the same
// compiled code at no added validator coverage. Don't cap below testLoopCount
// - with 8 hot callees and a Debug build, FTL tier-up needs the full budget.
const iterations = testLoopCount;
let sum = 0;
for (let i = 0; i < iterations; ++i) {
    sum += uint32Direct(u32, i);
    sum += phiIntDouble(4, i & 1);
    sum += toLengthOnUint32(u32, i);
    sum += truncatedArith(u32, i);
    sum += poisonTruncatedU32(u32, i);
    sum += uint32AsIndex(u32Mixed, dataArr, i);
    sum += divThenTrunc(100 + i, 3);
    sum += nested(dataArr, 8);
}

if (!Number.isFinite(sum))
    throw new Error("non-finite sum: " + sum);

// Spot-check exact values: any silent miscompile would change these.
assert(uint32Direct(u32, 0), 0xFFFFFFF0 + 1, "uint32Direct[0]");
assert(uint32Direct(u32, 7), 0xFFFFFFF7 + 1, "uint32Direct[7]");
assert(truncatedArith(u32, 0), (0xFFFFFFF0 | 0) + 1, "truncatedArith[0]");
assert(truncatedArith(u32, 7), (0xFFFFFFF7 | 0) + 1, "truncatedArith[7]");
assert(poisonTruncatedU32(u32, 0), 0xFFFFFFF0 | 0, "poisonTruncatedU32[0]");
assert(poisonTruncatedU32(u32, 7), 0xFFFFFFF7 | 0, "poisonTruncatedU32[7]");
assert(uint32AsIndex(u32Mixed, dataArr, 0), 0, "uint32AsIndex valid index");
assert(uint32AsIndex(u32Mixed, dataArr, 3), 3, "uint32AsIndex valid index high");
assert(uint32AsIndex(u32Mixed, dataArr, 4), -1, "uint32AsIndex out-of-range Double");
assert(divThenTrunc(100, 3), (100 / 3 | 0) + 1, "divThenTrunc 100/3");
assert(nested(dataArr, 4), 10, "nested[0..3]");

// And one truthful intermediate: phiIntDouble with both branches active.
assert(phiIntDouble(4, false), 4, "phiIntDouble all-int");
assert(phiIntDouble(4, true), 2, "phiIntDouble all-double");

for (const name of ["uint32Direct", "phiIntDouble", "toLengthOnUint32",
                    "truncatedArith", "poisonTruncatedU32", "uint32AsIndex",
                    "divThenTrunc", "nested"]) {
    if (!didFTLCompile[name])
        throw new Error(name + " never reached FTL - adversarial invariant untested for this case");
}
