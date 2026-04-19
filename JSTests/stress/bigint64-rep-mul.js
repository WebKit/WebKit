// Test BigInt64 representation: mul with int64-range values, overflow handling.
// Specifically tests that:
// 1. FTL ArithMul uses the CheckMul result directly (Bug Fix #1)
// 2. Overflow causes correct OSR exit with BigInt64Overflow

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- Basic multiply correctness ---
function mul(a, b) {
    return a * b;
}
noInline(mul);

// Warm up with values that fit in int64
for (let i = 1n; i <= 10000n; i++) {
    let result = mul(i, 2n);
    assert(result === i * 2n, `mul by 2 failed at i=${i}`);
}

// Commutative check
for (let i = 0; i < 1000; i++) {
    let a = BigInt(i);
    let b = BigInt(i + 7);
    assert(mul(a, b) === mul(b, a), `commutativity failed at i=${i}`);
}

// --- Zero multiplicand ---
function mulZero(a) {
    return a * 0n;
}
noInline(mulZero);

for (let i = 0; i < 10000; i++) {
    assert(mulZero(BigInt(i)) === 0n, `mul by zero failed at i=${i}`);
}

// --- Large result overflow (triggers OSR exit from BigInt64) ---
function mulOverflow(a, b) {
    return a * b;
}
noInline(mulOverflow);

// Warm up with small values first
for (let i = 1n; i <= 5000n; i++)
    mulOverflow(i, i);

// Now multiply values that overflow int64
let big = 1000000000n * 1000000000n * 100n; // 10^20, exceeds int64
let result = mulOverflow(big, 1n);
assert(result === big, "big multiply identity failed");

let result2 = mulOverflow(1000000000n, 1000000000n);
assert(result2 === 1000000000000000000n, "1e9 * 1e9 failed");

// INT64 overflow: 2^32 * 2^32 = 2^64 which overflows int64
let r = mulOverflow(4294967296n, 4294967296n);
assert(r === 18446744073709551616n, "2^64 failed");

// --- Negative multiplication ---
function mulNeg(a, b) {
    return a * b;
}
noInline(mulNeg);

for (let i = 1; i <= 5000; i++) {
    let result = mulNeg(BigInt(i), -1n);
    assert(result === -BigInt(i), `neg mul failed at i=${i}`);
}

// Warm up, then test double-negative
for (let i = 1; i <= 5000; i++) {
    let result = mulNeg(-BigInt(i), -1n);
    assert(result === BigInt(i), `double neg mul failed at i=${i}`);
}

print("PASS: bigint64-rep-mul");
