// Test BigInt64 representation: add/sub with int64-range values across all JIT tiers.
// Verifies that DFG/FTL correctly optimizes small HeapBigInt arithmetic without boxing/unboxing
// overhead, and that OSR exit works correctly when overflow is detected.

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- Basic add/sub correctness ---
function addSub(a, b) {
    return [a + b, a - b];
}
noInline(addSub);

// Warm up with values that fit in int64
for (let i = 0; i < 10000; i++) {
    let a = BigInt(i);
    let b = BigInt(i + 1);
    let [sum, diff] = addSub(a, b);
    assert(sum === BigInt(2 * i + 1), `add failed at i=${i}`);
    assert(diff === -1n, `sub failed at i=${i}`);
}

// --- Large values: verify graceful fallback (OSR exit) ---
function addLarge(a, b) {
    return a + b;
}
noInline(addLarge);

// First, warm up with small values to trigger BigInt64 speculation
for (let i = 0; i < 5000; i++) {
    let result = addLarge(BigInt(i), 1n);
    assert(result === BigInt(i + 1));
}

// Then use a value larger than int64 — this should trigger OSR exit if BigInt64 was speculated
let large = 2n ** 70n;
let r = addLarge(large, 1n);
assert(r === 2n ** 70n + 1n, "large BigInt add failed");

// --- Commutative check ---
function commutativeCheck(a, b) {
    return [a + b, b + a];
}
noInline(commutativeCheck);

for (let i = 0; i < 10000; i++) {
    let a = BigInt(i * 7);
    let b = BigInt(i * 3 + 1);
    let [ab, ba] = commutativeCheck(a, b);
    assert(ab === ba, `commutativity failed at i=${i}`);
}

// --- Negative values ---
function negativeArith(a) {
    return a + (-1n) - 100n;
}
noInline(negativeArith);

for (let i = 0; i < 10000; i++) {
    let result = negativeArith(BigInt(i));
    assert(result === BigInt(i) - 101n, `negative arith failed at i=${i}`);
}

// --- Mixed int64-range boundary values ---
const INT64_MAX = 9223372036854775807n;
const INT64_MIN = -9223372036854775808n;

function boundaryAdd(a, b) {
    return a + b;
}
noInline(boundaryAdd);

// Warm up
for (let i = 0; i < 1000; i++)
    boundaryAdd(BigInt(i), BigInt(i));

// Boundary: INT64_MAX + 1 should overflow and trigger OSR exit
let overflow = boundaryAdd(INT64_MAX, 1n);
assert(overflow === INT64_MAX + 1n, "INT64_MAX + 1 wrong");

// Boundary: INT64_MIN - 1 should overflow
let underflow = boundaryAdd(INT64_MIN, -1n);
assert(underflow === INT64_MIN - 1n, "INT64_MIN - 1 wrong");

print("PASS: bigint64-rep-add");
