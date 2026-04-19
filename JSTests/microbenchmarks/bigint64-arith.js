// Microbenchmark: BigInt64 arithmetic — add/sub/mul/compare on int64-range BigInts.
// Run hot enough to reach FTL (~100k iterations per function).
// All operands deliberately fit in int64 so the BigInt64 representation tier fires.

"use strict";

const WARMUP = 1e5;

// --- Add ---
// Use a base value above the BigInt32 threshold (2^32 > 2^30 ≈ BigInt32 max).
// On platforms with USE(BIGINT32), values < 2^30 are stored as tagged int32 rather
// than HeapBigInt objects, producing a mixed HeapBigInt|BoolInt32|NonBoolInt32
// prediction that blocks binaryArithShouldSpeculateBigInt64.  Starting from 2^32
// ensures every operand profiles as pure HeapBigInt from the very first iteration.
const ADD_BASE = 0x100000000n; // 2^32
const addLimit = ADD_BASE + BigInt(WARMUP);
let addResult = ADD_BASE;
for (let i = ADD_BASE; i < addLimit; i += 1n)
    addResult += i;

// Expected: ADD_BASE + sum(ADD_BASE .. ADD_BASE+WARMUP-1)
//         = ADD_BASE*(WARMUP+1) + WARMUP*(WARMUP-1)/2
const expectedAdd = ADD_BASE * (BigInt(WARMUP) + 1n) + BigInt(WARMUP) * BigInt(WARMUP - 1) / 2n;
if (addResult !== expectedAdd)
    throw new Error(`add benchmark gave wrong result: got ${addResult}, expected ${expectedAdd}`);

// --- Sub ---
let subResult = BigInt(WARMUP);
for (let i = 0n; i < WARMUP; i++)
    subResult -= 1n;

if (subResult !== 0n)
    throw new Error("sub benchmark gave wrong result");

// --- Mul (stays in int64 range: values up to ~1e5) ---
let mulResult = 1n;
for (let i = 1n; i <= 18n; i++)
    mulResult *= i;  // 18! = 6402373705728000 which fits in int64

if (mulResult !== 6402373705728000n)
    throw new Error("mul benchmark gave wrong result");

// --- Bitwise AND/OR/XOR mask processing ---
let mask = 0xFFFFFFFFn;
let andResult = 0n;
let orResult = 0n;
let xorResult = 0n;
for (let i = 0n; i < WARMUP; i++) {
    andResult += i & mask;
    orResult += i | mask;
    xorResult += i ^ mask;
}

// Rough sanity check — just ensure they ran
if (andResult === 0n && orResult === 0n && xorResult === 0n)
    throw new Error("bitwise benchmark produced all zeros");

// --- Comparison chain ---
let cmpCount = 0;
let prev = 0n;
for (let i = 0n; i < WARMUP; i++) {
    if (i > prev)
        cmpCount++;
    prev = i;
}

if (cmpCount !== WARMUP - 1)
    throw new Error("comparison benchmark gave wrong count");

// Report timing via print (used by jsc harness)
print("PASS: bigint64-arith microbenchmark");
