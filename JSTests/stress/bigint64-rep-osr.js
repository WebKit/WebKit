// Test BigInt64 OSR entry/exit round-trip correctness.
// Verifies that values are correctly boxed/unboxed when transitioning between
// interpreter and DFG/FTL tiers (DFGOSREntry.cpp and DFGOSRExit.cpp paths).

"use strict";

function assert(cond, msg) {
    if (!cond)
        throw new Error(msg || "assertion failed");
}

// --- OSR entry: function already compiled as BigInt64, called with int64-range BigInts ---
function osrEntryAdd(n) {
    let sum = 0n;
    for (let i = 0n; i < n; i++)
        sum += i;
    return sum;
}
noInline(osrEntryAdd);

// Warm up to trigger compilation
let result = osrEntryAdd(10000n);
// sum 0..9999 = n*(n-1)/2
assert(result === 10000n * 9999n / 2n, `osrEntryAdd failed: got ${result}`);

// --- OSR exit: function compiled as BigInt64, but loop encounters large BigInt ---
function osrExitOnLarge(arr) {
    let sum = 0n;
    for (let i = 0; i < arr.length; i++)
        sum += arr[i];
    return sum;
}
noInline(osrExitOnLarge);

// Build array with small BigInts to trigger BigInt64 compilation
let smallArr = [];
for (let i = 0; i < 1000; i++)
    smallArr.push(BigInt(i));

// Warm up
for (let j = 0; j < 10; j++)
    osrExitOnLarge(smallArr);

// Expected sum
let expectedSmall = smallArr.reduce((a, b) => a + b, 0n);
assert(osrExitOnLarge(smallArr) === expectedSmall, "small array sum wrong");

// Now trigger OSR exit by including a large BigInt mid-array
let mixedArr = [...smallArr, 2n ** 70n];
let expectedMixed = expectedSmall + 2n ** 70n;
let resultMixed = osrExitOnLarge(mixedArr);
assert(resultMixed === expectedMixed, `mixed array sum wrong: got ${resultMixed}`);

// --- Tier re-entry: after OSR exit, function should still work correctly ---
// Run the small array again to verify re-compilation with correct types
for (let j = 0; j < 100; j++) {
    let r = osrExitOnLarge(smallArr);
    assert(r === expectedSmall, `post-exit small array sum wrong at j=${j}`);
}

// --- Value preservation across OSR: local BigInt64 variable must survive ---
function localBigInt64Survival(n) {
    let a = 42n;       // should become BigInt64 in DFG
    let b = 100n;      // same
    for (let i = 0; i < n; i++) {
        a = a + b;
        if (i === 50)
            b = b + 0n;  // no-op but touches b
    }
    return a;
}
noInline(localBigInt64Survival);

// 42 + 100 * 10000 = 1000042
let survivalResult = localBigInt64Survival(10000);
assert(survivalResult === 42n + 100n * 10000n, `local survival failed: got ${survivalResult}`);

// --- Cross-tier value correctness: return BigInt64 from DFG to interpreter ---
function bigIntReturn(n) {
    return BigInt(n) * BigInt(n);
}
noInline(bigIntReturn);

for (let i = 0; i < 10000; i++) {
    let r = bigIntReturn(i);
    assert(r === BigInt(i) * BigInt(i), `bigIntReturn failed at i=${i}`);
}

// Verify identity for INT64_MAX
const INT64_MAX = 9223372036854775807n;
function identityBigInt64(v) {
    return v + 0n;
}
noInline(identityBigInt64);

for (let i = 0; i < 1000; i++)
    identityBigInt64(BigInt(i));

assert(identityBigInt64(INT64_MAX) === INT64_MAX, "INT64_MAX identity failed");

print("PASS: bigint64-rep-osr");
