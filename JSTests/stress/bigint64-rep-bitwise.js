// Test BigInt64 representation: bitwise operations and/or/xor/not/shift.
// Verifies DFG/FTL BigInt64 bitwise codegen; also validates Bug Fix #4 (ArithBitNot).

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- Bitwise AND ---
function bitwiseAnd(a, b) {
    return a & b;
}
noInline(bitwiseAnd);

for (let i = 0n; i < 10000n; i++) {
    assert(bitwiseAnd(i, 0n) === 0n, `AND 0 failed at ${i}`);
    assert(bitwiseAnd(i, -1n) === i, `AND -1 failed at ${i}`);
    assert(bitwiseAnd(i, i) === i, `AND self failed at ${i}`);
}

// --- Bitwise OR ---
function bitwiseOr(a, b) {
    return a | b;
}
noInline(bitwiseOr);

for (let i = 0n; i < 10000n; i++) {
    assert(bitwiseOr(i, 0n) === i, `OR 0 failed at ${i}`);
    assert(bitwiseOr(i, i) === i, `OR self failed at ${i}`);
}

// --- Bitwise XOR ---
function bitwiseXor(a, b) {
    return a ^ b;
}
noInline(bitwiseXor);

for (let i = 0n; i < 10000n; i++) {
    assert(bitwiseXor(i, 0n) === i, `XOR 0 failed at ${i}`);
    assert(bitwiseXor(i, i) === 0n, `XOR self failed at ${i}`);
}

// --- Bitwise NOT (Bug Fix #4: FTL ArithBitNot must handle BigInt64RepUse) ---
function bitwiseNot(a) {
    return ~a;
}
noInline(bitwiseNot);

for (let i = 0n; i < 10000n; i++) {
    let result = bitwiseNot(i);
    // ~i for BigInt = -(i + 1)
    assert(result === -(i + 1n), `NOT failed at ${i}: got ${result}, expected ${-(i + 1n)}`);
}

// Double NOT should return original
for (let i = 0n; i < 5000n; i++) {
    assert(bitwiseNot(bitwiseNot(i)) === i, `double NOT failed at ${i}`);
}

// --- Left shift ---
function lshift(a, b) {
    return a << b;
}
noInline(lshift);

for (let i = 0n; i < 60n; i++) {
    let result = lshift(1n, i);
    assert(result === 1n << i, `lshift failed at i=${i}`);
}

// --- Right shift ---
function rshift(a, b) {
    return a >> b;
}
noInline(rshift);

for (let i = 0n; i < 60n; i++) {
    let val = 1n << 62n;
    let result = rshift(val, i);
    assert(result === val >> i, `rshift failed at i=${i}`);
}

// --- Mixed bitwise expression ---
function mixedBitwise(a, b) {
    return (a & b) | (a ^ b);
}
noInline(mixedBitwise);

for (let i = 0n; i < 10000n; i++) {
    let b = i ^ 0xFFFFn;
    // (a & b) | (a ^ b) = a | b
    assert(mixedBitwise(i, b) === (i | b), `mixed bitwise failed at i=${i}`);
}

print("PASS: bigint64-rep-bitwise");
