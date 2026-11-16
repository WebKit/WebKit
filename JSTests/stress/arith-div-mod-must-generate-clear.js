// Regression test for clearing NodeMustGenerate on ArithDiv / ArithMod
// in DFGFixupPhase (for Int32 / Int52 / DoubleRep inputs), together with
// DFGMayExit treating Arith::Unchecked Int32 / Int52 Div/Mod as non-exiting
// and StrengthReduction switching ArithMod(x, power-of-two) from
// CheckOverflow to Unchecked.
//
// The DCE-after-fixup behavior must preserve observable semantics: if the
// result is used anywhere (including an observable operand conversion), the
// value is correct; if the result is unused, the program must not diverge
// from the pre-optimization behavior (in particular: no spurious exceptions,
// no stale values on uses, and the OSR exit machinery must still agree with
// mayExit).

function assertEq(actual, expected, label) {
    if (actual !== expected)
        throw new Error(`${label}: expected ${expected}, got ${actual}`);
}

function assertNegZero(value, label) {
    if (value !== 0 || 1 / value !== -Infinity)
        throw new Error(`${label}: expected -0, got ${value}`);
}

function assertPosZero(value, label) {
    if (value !== 0 || 1 / value !== Infinity)
        throw new Error(`${label}: expected +0, got ${value}`);
}

// ===== Unused Int32 ArithDiv with Unchecked mode =====
// (x / y) | 0 is truncate-to-int32 + can-ignore-NaN/Inf + can-ignore-negZero,
// so FixupPhase sets Arith::Unchecked on the ArithDiv. With NodeMustGenerate
// cleared, the outer | 0 consumes the div; here we still read the result so
// we can verify correctness. The sibling test below drops the result to
// exercise DCE.
function divInt32Unchecked(a, b) {
    return (a / b) | 0;
}
noInline(divInt32Unchecked);

for (let i = 0; i < testLoopCount; ++i) {
    assertEq(divInt32Unchecked(42, 5), 8, "divInt32Unchecked normal");
    // chillDiv: x / 0 = 0 in unchecked mode, | 0 preserves 0
    assertEq(divInt32Unchecked(42, 0), 0, "divInt32Unchecked zero divisor");
    // INT_MIN / -1 would overflow in checked mode; chillDiv returns numerator here.
    assertEq(divInt32Unchecked(-2147483648, -1), -2147483648 | 0, "divInt32Unchecked INT_MIN / -1");
}

// ===== Unused Int32 ArithDiv / ArithMod — DCE-visible pattern =====
// The result is assigned to a local that's never read. With the fix, DCE
// removes the ArithDiv/ArithMod. We verify the function still returns the
// expected value (so we know no OSR exit was forced) and that calls with
// zero divisors don't throw / don't corrupt anything.
function divUnusedResult(a, b) {
    var unused = (a / b) | 0;
    return a + b;
}
noInline(divUnusedResult);

function modUnusedResult(a, b) {
    var unused = (a % b) | 0;
    return a + b;
}
noInline(modUnusedResult);

for (let i = 0; i < testLoopCount; ++i) {
    assertEq(divUnusedResult(17, 3), 20, "divUnusedResult normal");
    assertEq(divUnusedResult(17, 0), 17, "divUnusedResult zero divisor");
    assertEq(divUnusedResult(-2147483648, -1), -2147483649, "divUnusedResult INT_MIN / -1");

    assertEq(modUnusedResult(17, 3), 20, "modUnusedResult normal");
    assertEq(modUnusedResult(17, 0), 17, "modUnusedResult zero divisor");
    assertEq(modUnusedResult(-2147483648, -1), -2147483649, "modUnusedResult INT_MIN / -1");
}

// ===== Unused Int52 ArithMod — DCE-visible pattern =====
// Int52 path in fixupArithDiv: when modShouldSpeculateInt52 and the
// bytecode flags allow it, mode is Unchecked and MustGenerate is cleared.
function modInt52Unused(a, b) {
    var unused = (a % b) | 0;
    return a;
}
noInline(modInt52Unused);

for (let i = 0; i < testLoopCount; ++i) {
    assertEq(modInt52Unused(2147483648, 7), 2147483648, "modInt52Unused normal");
    assertEq(modInt52Unused(2147483648, 0), 2147483648, "modInt52Unused zero divisor");
    assertEq(modInt52Unused(-4503599627370496, 1000000007), -4503599627370496, "modInt52Unused large negative");
}

// ===== Unused Double ArithDiv / ArithMod — DCE-visible pattern =====
// fixupArithDiv non-integer fallback: setResult(NodeResultDouble) + clearFlags.
function divDoubleUnused(a, b) {
    var unused = a / b;
    return a + b;
}
noInline(divDoubleUnused);

function modDoubleUnused(a, b) {
    var unused = a % b;
    return a + b;
}
noInline(modDoubleUnused);

for (let i = 0; i < testLoopCount; ++i) {
    assertEq(divDoubleUnused(3.5, 2.0), 5.5, "divDoubleUnused normal");
    assertEq(divDoubleUnused(3.5, 0.0), 3.5, "divDoubleUnused zero divisor produces Infinity-but-unused");
    assertEq(modDoubleUnused(3.5, 2.0), 5.5, "modDoubleUnused normal");
    assertEq(modDoubleUnused(3.5, 0.0), 3.5, "modDoubleUnused zero divisor produces NaN-but-unused");
}

// ===== Operand side-effects must still fire when result is DCE'd =====
// The operands of ArithDiv/ArithMod are int32 edges; the valueOf is called
// during ToInt32 conversion which happens BEFORE the ArithDiv. Even if DCE
// removes the ArithDiv itself, the valueOf-driven conversions must remain
// because they are separate nodes in the DFG.
let sideEffectCount = 0;
const sideEffectOperand = {
    valueOf: function () {
        sideEffectCount++;
        return 7;
    }
};

function divWithSideEffectOperand(obj, b) {
    // (obj / b) | 0 — the |0 keeps ValueDiv's operand conversions alive
    // through ToNumber/ToInt32. Even if the div itself is DCE'd, we don't
    // expect the conversions to vanish because they're separate nodes.
    var unused = (obj / b) | 0;
    return obj + 0; // force another coerce so valueOf count is predictable
}
noInline(divWithSideEffectOperand);

sideEffectCount = 0;
for (let i = 0; i < testLoopCount; ++i)
    divWithSideEffectOperand(sideEffectOperand, 2);
// obj.valueOf is called at least for the return's `obj + 0`. The div may or
// may not call it depending on whether the op stays ValueDiv or gets fixed
// to Int32 with the operand pre-converted. Either way, count must be >= the
// loop count (at least one call per iteration for the `obj + 0`).
if (sideEffectCount < testLoopCount)
    throw new Error(`divWithSideEffectOperand side effect count ${sideEffectCount} < ${testLoopCount}`);

// ===== Strength reduction: ArithMod(x, power-of-two) in CheckOverflow mode =====
// When mode is CheckOverflow (negative-zero doesn't matter but overflow
// might), strength reduction now rewrites power-of-two divisors to
// Arith::Unchecked because they can never divide by zero and can never hit
// the INT_MIN/-1 trap. The produced value must still be correct.
function modPow2InCheckOverflow(a, k) {
    // Return a non-bitwise result so the caller's observed value preserves
    // NaN/Infinity. But keep the divisor constant and power-of-two so the
    // strength-reduction rule fires.
    if (k === 256)
        return a % 256;
    if (k === 1024)
        return a % 1024;
    if (k === 65536)
        return a % 65536;
    return a % 2;
}
noInline(modPow2InCheckOverflow);

const pow2TestValues = [0, 1, -1, 2, -2, 128, -128, 255, -255, 256, -256,
    1023, -1024, 65535, -65536, 2147483647, -2147483648, -2147483647];

for (let i = 0; i < testLoopCount; ++i) {
    for (const v of pow2TestValues) {
        assertEq(modPow2InCheckOverflow(v, 256), v % 256, `modPow2 ${v} % 256`);
        assertEq(modPow2InCheckOverflow(v, 1024), v % 1024, `modPow2 ${v} % 1024`);
        assertEq(modPow2InCheckOverflow(v, 65536), v % 65536, `modPow2 ${v} % 65536`);
    }
}

// ===== Strength reduction: power-of-two must NOT switch to Unchecked when
// negative-zero matters (CheckOverflowAndNegativeZero). The optimization
// only fires on CheckOverflow. Here the result is observed as -0, so
// negative-zero speculation must still be in effect. =====
function modPow2PreservesNegZero(a) {
    // -0 would be produced by (-X) % 256 when X is a multiple of 256.
    // The caller checks 1/result, forcing negative-zero observation.
    return a % 256;
}
noInline(modPow2PreservesNegZero);

for (let i = 0; i < testLoopCount; ++i) {
    // Negative multiple of 256 produces -0.
    assertNegZero(modPow2PreservesNegZero(-256), "modPow2PreservesNegZero -256");
    assertNegZero(modPow2PreservesNegZero(-512), "modPow2PreservesNegZero -512");
    assertNegZero(modPow2PreservesNegZero(-65536), "modPow2PreservesNegZero -65536");
    // Positive multiples produce +0.
    assertPosZero(modPow2PreservesNegZero(256), "modPow2PreservesNegZero 256");
    assertPosZero(modPow2PreservesNegZero(512), "modPow2PreservesNegZero 512");
    // Non-multiples: normal remainder.
    assertEq(modPow2PreservesNegZero(-257), -1, "modPow2PreservesNegZero -257");
    assertEq(modPow2PreservesNegZero(257), 1, "modPow2PreservesNegZero 257");
}

// ===== Strength reduction: non-power-of-two divisor must NOT switch =====
// const > 1 but not power-of-two must stay in CheckOverflow (or whatever
// mode FixupPhase set). Correctness check: divisor 3 behaves as integer mod.
function modNonPow2(a) {
    return a % 3;
}
noInline(modNonPow2);

for (let i = 0; i < testLoopCount; ++i) {
    assertEq(modNonPow2(10), 1, "modNonPow2 10 % 3");
    assertEq(modNonPow2(-10), -1, "modNonPow2 -10 % 3");
    assertEq(modNonPow2(0), 0, "modNonPow2 0 % 3");
}

// ===== IntegerRangeOptimization interaction =====
// After the MustGenerate-clear, the IntegerRangeOptimizationPhase must not
// draw conclusions from the *existence* of an ArithDiv/ArithMod check
// (because the node may have been DCE'd). This loop pattern is the classic
// shape that drove the invariant: the mod result bounds the index, the
// array access still needs its own bounds check.
function rangeOptArrayMod(arr) {
    let sum = 0;
    for (let i = 0; i < arr.length; ++i) {
        const idx = i % arr.length;
        sum += arr[idx];
    }
    return sum;
}
noInline(rangeOptArrayMod);

const rangeArr = [1, 2, 3, 4, 5, 6, 7, 8];
const rangeExpected = 36;
for (let i = 0; i < testLoopCount; ++i) {
    assertEq(rangeOptArrayMod(rangeArr), rangeExpected, "rangeOptArrayMod");
}

// ===== Typed-array store elision + DCE of the mod node =====
// This is the end-to-end scenario motivating the patch: DFGStrengthReduction
// rewires PutByVal(Uint8Array, ArithMod(x, 256)) to PutByVal(Uint8Array, x).
// Previously the ArithMod survived DCE because of NodeMustGenerate; now it
// is removed. Correctness guard: the stored value must equal x & 0xff.
function storeModUnused(arr, i, x) {
    arr[i] = x % 256;
}
noInline(storeModUnused);

const storeArr = new Uint8Array(4);
for (let i = 0; i < testLoopCount; ++i) {
    storeModUnused(storeArr, 0, 0x1234);
    storeModUnused(storeArr, 1, -1);
    storeModUnused(storeArr, 2, 2147483647);
    storeModUnused(storeArr, 3, -2147483648);
}
assertEq(storeArr[0], 0x34, "storeModUnused 0x1234 -> low byte");
assertEq(storeArr[1], 0xff, "storeModUnused -1 -> 0xff");
assertEq(storeArr[2], 0xff, "storeModUnused INT32_MAX -> 0xff");
assertEq(storeArr[3], 0x00, "storeModUnused INT32_MIN -> 0");

// ===== Shared ArithMod: store to Uint8Array + other use =====
// StrengthReduction rewires only the store's value edge; the other use
// still needs the modded value. MustGenerate-clear doesn't change this
// (the other use keeps the node alive via its edge).
function sharedMod(arr, i, x) {
    const m = x % 256;
    arr[i] = m;
    return m;
}
noInline(sharedMod);

const sharedArr = new Uint8Array(1);
for (let i = 0; i < testLoopCount; ++i) {
    const r = sharedMod(sharedArr, 0, 0x1234);
    // x % 256 in JS: 4660 % 256 = 52 (=0x34)
    assertEq(r, 52, "sharedMod return 0x1234 % 256");
    assertEq(sharedArr[0], 52, "sharedMod store 0x1234 low byte");

    const r2 = sharedMod(sharedArr, 0, -1);
    assertEq(r2, -1, "sharedMod return -1 % 256");
    assertEq(sharedArr[0], 0xff, "sharedMod store -1 low byte");
}

// ===== Nested ArithMod with power-of-two: strength reduction fires
// repeatedly because the outer mod's divisor is a multiple of the inner's.
// The existing identity-conversion rule still applies and must continue to
// work after the refactor. =====
function nestedModPow2(x) {
    return (x % 256) % 16;
}
noInline(nestedModPow2);

for (let i = 0; i < testLoopCount; ++i) {
    for (const v of pow2TestValues)
        assertEq(nestedModPow2(v), (v % 256) % 16, `nestedModPow2 ${v}`);
}

// ===== FTL warm-up: very hot loop that drives the node into FTL where
// LICM + IntegerRangeOptimization + MovHintRemoval all consult mayExit.
// If any of them disagree with the Fixup/StrengthReduction story, we'd see
// divergent results or crashes here. =====
function ftlHotMod(seed) {
    let acc = 0;
    for (let i = 0; i < 1000; ++i) {
        const m = (seed + i) % 256;
        acc = (acc + m) | 0;
    }
    return acc;
}
noInline(ftlHotMod);

let ftlResult = 0;
for (let i = 0; i < testLoopCount; ++i)
    ftlResult = ftlHotMod(i);

// Recompute the final-iteration expected value explicitly.
{
    let acc = 0;
    const seed = testLoopCount - 1;
    for (let i = 0; i < 1000; ++i) {
        const m = (seed + i) % 256;
        acc = (acc + m) | 0;
    }
    assertEq(ftlResult, acc, "ftlHotMod final iteration");
}
