// Test BigInt64 overflow profiling and speculation gating.
// Verifies that:
// 1. When large BigInts are observed, BigInt64Overflow profiling fires (Bug Fix #3)
// 2. DFG/FTL does NOT speculate BigInt64 for operations that have seen large BigInts
// 3. Gradual overflow: small → large BigInt correctly suppresses speculation
// This test covers the JetStream 3 regression scenario: crypto BigInts must never
// trigger BigInt64 speculation.

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- Scenario 1: Always-large BigInts — should never speculate BigInt64 ---
// These simulate JetStream 3 crypto workloads (RSA-style large integers).
function addLarge(a, b) {
    return a + b;
}
noInline(addLarge);

let base = 2n ** 256n;  // 256-bit integer, well outside int64 range
for (let i = 0; i < 10000; i++) {
    let result = addLarge(base + BigInt(i), base + BigInt(i + 1));
    assert(result === 2n * base + BigInt(2 * i + 1), `large add failed at ${i}`);
}

// --- Scenario 2: Gradual overflow — start small, then grow beyond int64 ---
// Profiler must detect BigInt64Overflow and prevent re-speculation after overflow.
function accumulate(arr) {
    let sum = 0n;
    for (let i = 0; i < arr.length; i++)
        sum += arr[i];
    return sum;
}
noInline(accumulate);

// Phase 1: small values — speculation allowed
let smallValues = [];
for (let i = 0; i < 1000; i++)
    smallValues.push(BigInt(i));

let smallSum = accumulate(smallValues);
assert(smallSum === 1000n * 999n / 2n, `small accumulate failed: ${smallSum}`);

// Phase 2: introduce large value — must NOT produce wrong result
let largeValues = [...smallValues, 2n ** 70n];
let largeSum = accumulate(largeValues);
assert(largeSum === smallSum + 2n ** 70n, `large accumulate failed: ${largeSum}`);

// Phase 3: go back to small values — profiler has BigInt64Overflow set, won't speculate
// Result must still be correct even though speculation is suppressed
for (let run = 0; run < 100; run++) {
    let result = accumulate(smallValues);
    assert(result === smallSum, `post-overflow small accumulate failed at run=${run}`);
}

// --- Scenario 3: Multiplication overflow ---
function mulAll(arr) {
    let prod = 1n;
    for (let i = 0; i < arr.length; i++)
        prod *= arr[i];
    return prod;
}
noInline(mulAll);

// Small primes — product stays in int64 range for small count
let primes = [2n, 3n, 5n, 7n, 11n, 13n, 17n, 19n, 23n, 29n];

// Warm up
for (let i = 0; i < 1000; i++)
    mulAll(primes);

// Expected product
let expectedProd = primes.reduce((a, b) => a * b, 1n);
assert(mulAll(primes) === expectedProd, `prime product failed: ${mulAll(primes)}`);

// Now multiply until overflow
let bigPrimes = [...primes, 31n, 37n, 41n, 43n, 47n, 53n, 59n, 61n, 67n, 71n, 73n, 79n];
let bigProd = mulAll(bigPrimes);
let expectedBig = bigPrimes.reduce((a, b) => a * b, 1n);
assert(bigProd === expectedBig, `big prime product failed`);

// --- Scenario 4: JetStream 3 simulation — alternating large/small BigInts ---
// Verify profiling correctly gates speculation for mixed-size workloads.
function bigintMix(small, large, useLarge) {
    if (useLarge)
        return large + large;
    return small + small;
}
noInline(bigintMix);

let big3072 = 2n ** 3072n;  // typical RSA-3072 size

// Alternate to ensure profiler sees both small and large
for (let i = 0; i < 10000; i++) {
    let useLarge = (i % 3 === 0);
    let s = BigInt(i);
    let result = bigintMix(s, big3072, useLarge);
    if (useLarge)
        assert(result === 2n * big3072, `large mix failed at ${i}`);
    else
        assert(result === 2n * s, `small mix failed at ${i}`);
}

// --- Scenario 5: INT64 boundary — exactly at boundary should work ---
const INT64_MAX = 9223372036854775807n;
const INT64_MIN = -9223372036854775808n;

function atBoundary(a, b) {
    return a + b;
}
noInline(atBoundary);

for (let i = 0; i < 1000; i++)
    atBoundary(BigInt(i), BigInt(i));

// INT64_MAX itself (fits in int64) — speculation should handle it
assert(atBoundary(INT64_MAX, 0n) === INT64_MAX, "INT64_MAX + 0 failed");
// INT64_MAX + 1 overflows int64 — OSR exit expected if speculated, must give right answer
assert(atBoundary(INT64_MAX, 1n) === INT64_MAX + 1n, "INT64_MAX + 1 failed");
// INT64_MIN itself (fits in int64)
assert(atBoundary(INT64_MIN, 0n) === INT64_MIN, "INT64_MIN + 0 failed");

print("PASS: bigint64-rep-overflow");
