// Test fast path for double modulo with positive integers.
// Ported from V8 regression test for commit 40e82593f5.

function doMod(a, b) {
    return a % b;
}
noInline(doMod);

// Run enough iterations to trigger DFG/FTL compilation.
for (var i = 0; i < 100000; i++) {
    // Positive integer cases (fast path).
    var result = doMod(10, 3);
    if (result !== 1)
        throw new Error("Expected 10 % 3 === 1, got " + result);

    result = doMod(7, 4);
    if (result !== 3)
        throw new Error("Expected 7 % 4 === 3, got " + result);

    result = doMod(100, 7);
    if (result !== 2)
        throw new Error("Expected 100 % 7 === 2, got " + result);

    // Large positive integers where precision matters.
    result = doMod(2147483647, 10);
    if (result !== 7)
        throw new Error("Expected 2147483647 % 10 === 7, got " + result);

    result = doMod(1073741824, 3);
    if (result !== 1)
        throw new Error("Expected 1073741824 % 3 === 1, got " + result);

    // Non-integer cases (slow path).
    result = doMod(5.5, 2);
    if (result !== 1.5)
        throw new Error("Expected 5.5 % 2 === 1.5, got " + result);

    result = doMod(10, 3.5);
    if (result !== 3)
        throw new Error("Expected 10 % 3.5 === 3, got " + result);

    // Negative cases (slow path).
    result = doMod(-10, 3);
    if (result !== -1)
        throw new Error("Expected -10 % 3 === -1, got " + result);

    result = doMod(10, -3);
    if (result !== 1)
        throw new Error("Expected 10 % -3 === 1, got " + result);

    // Zero dividend (slow path).
    result = doMod(0, 5);
    if (result !== 0)
        throw new Error("Expected 0 % 5 === 0, got " + result);

    // Very large integers beyond 2^53 must use slow path for correctness.
    // 2^100 % 3 === 1 (since 2^even ≡ 1 mod 3).
    result = doMod(Math.pow(2, 100), 3);
    if (result !== 1)
        throw new Error("Expected 2^100 % 3 === 1, got " + result);

    // Near the safe integer boundary.
    result = doMod(9007199254740991, 7);
    if (result !== 3)
        throw new Error("Expected (2^53-1) % 7 === 3, got " + result);
}

// Additional large value test from V8.
function testLargeValues() {
    return 3.5e15 % 100;
}
noInline(testLargeValues);

for (var i = 0; i < 100000; i++) {
    var result = testLargeValues();
    if (result !== 0)
        throw new Error("Expected 3.5e15 % 100 === 0, got " + result);
}
