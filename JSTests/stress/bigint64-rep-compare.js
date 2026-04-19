// Test BigInt64 representation: comparisons.
// Specifically validates Bug Fix #2: comparisonShouldSpeculateBigInt64 requires BOTH sides
// to be BigInt64, not just one.

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- Equal ---
function eq(a, b) { return a === b; }
noInline(eq);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i);
    let b = BigInt(i);
    let c = BigInt(i + 1);
    assert(eq(a, b) === true, `eq true failed at ${i}`);
    assert(eq(a, c) === false, `eq false failed at ${i}`);
}

// --- Not equal (via !=) ---
function neq(a, b) { return a !== b; }
noInline(neq);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i);
    assert(neq(a, BigInt(i + 1)) === true, `neq true failed at ${i}`);
    assert(neq(a, BigInt(i)) === false, `neq false failed at ${i}`);
}

// --- Less than ---
function lt(a, b) { return a < b; }
noInline(lt);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i);
    let b = BigInt(i + 1);
    assert(lt(a, b) === true, `lt true failed at ${i}`);
    assert(lt(b, a) === false, `lt false failed at ${i}`);
    assert(lt(a, a) === false, `lt eq failed at ${i}`);
}

// --- Less than or equal ---
function lte(a, b) { return a <= b; }
noInline(lte);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i);
    let b = BigInt(i + 1);
    assert(lte(a, b) === true, `lte lt failed at ${i}`);
    assert(lte(a, a) === true, `lte eq failed at ${i}`);
    assert(lte(b, a) === false, `lte gt failed at ${i}`);
}

// --- Greater than ---
function gt(a, b) { return a > b; }
noInline(gt);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i + 1);
    let b = BigInt(i);
    assert(gt(a, b) === true, `gt true failed at ${i}`);
    assert(gt(b, a) === false, `gt false failed at ${i}`);
    assert(gt(a, a) === false, `gt eq failed at ${i}`);
}

// --- Greater than or equal ---
function gte(a, b) { return a >= b; }
noInline(gte);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i + 1);
    let b = BigInt(i);
    assert(gte(a, b) === true, `gte gt failed at ${i}`);
    assert(gte(a, a) === true, `gte eq failed at ${i}`);
    assert(gte(b, a) === false, `gte lt failed at ${i}`);
}

// --- Bug Fix #2 regression: one BigInt64 side, one non-BigInt64 side ---
// This must NOT cause misspeculation; the comparison should work correctly.
function compareAsymmetric(a, b) {
    return a < b;
}
noInline(compareAsymmetric);

// Warm up with small BigInts
for (let i = 0; i < 5000; i++)
    compareAsymmetric(BigInt(i), BigInt(i + 1));

// Now try: one side is a large BigInt (won't fit in int64), other is small
let largeBigInt = 2n ** 70n;
assert(compareAsymmetric(1n, largeBigInt) === true, "asymmetric compare 1 < 2^70 failed");
assert(compareAsymmetric(largeBigInt, 1n) === false, "asymmetric compare 2^70 < 1 failed");
assert(compareAsymmetric(largeBigInt, largeBigInt) === false, "asymmetric compare 2^70 < 2^70 failed");

// --- Negative comparisons ---
function cmpNeg(a, b) { return a < b; }
noInline(cmpNeg);

for (let i = 0; i < 10000; i++) {
    assert(cmpNeg(-BigInt(i + 1), -BigInt(i)) === true, `neg lt failed at ${i}`);
    assert(cmpNeg(-BigInt(i), -BigInt(i + 1)) === false, `neg lt false failed at ${i}`);
}

// INT64 boundary comparisons
const INT64_MAX = 9223372036854775807n;
const INT64_MIN = -9223372036854775808n;

function boundaryCompare(a, b) { return a < b; }
noInline(boundaryCompare);

for (let i = 0; i < 1000; i++)
    boundaryCompare(BigInt(i), BigInt(i + 1));

assert(boundaryCompare(INT64_MIN, INT64_MAX) === true, "INT64_MIN < INT64_MAX failed");
assert(boundaryCompare(INT64_MAX, INT64_MIN) === false, "INT64_MAX < INT64_MIN failed");

print("PASS: bigint64-rep-compare");
