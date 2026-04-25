// Test Int52 ArithMod with Arith::Unchecked mode.
// When the result of Int52 modulo feeds into bitwise operations (like | 0),
// backward propagation clears NodeBytecodeNeedsNaNOrInfinity and NodeBytecodeNeedsNegZero,
// enabling Arith::Unchecked mode which avoids OSR exits on division-by-zero
// and negative-zero results.

// ===== Basic Int52 mod with | 0 (Arith::Unchecked path) =====

// The | 0 consumer clears NeedsNaNOrInfinity and NeedsNegZero on ArithMod,
// so ArithMod(Int52, Int52) gets Arith::Unchecked mode.
function modInt52BitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52BitOr);

// Basic Int52 mod with | 0
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitOr(2147483648, 7);
    if (result !== (2147483648 % 7))
        throw "FAIL: modInt52BitOr basic, got " + result + " expected " + (2147483648 % 7);
}

// Negative numerator
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitOr(-4503599627370496, 1000000007);
    let expected = (-4503599627370496 % 1000000007) | 0;
    if (result !== expected)
        throw "FAIL: modInt52BitOr negative numerator, got " + result + " expected " + expected;
}

// Large Int52 values
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitOr(4503599627370495, 97);
    let expected = (4503599627370495 % 97) | 0;
    if (result !== expected)
        throw "FAIL: modInt52BitOr large values, got " + result + " expected " + expected;
}

// ===== Division by zero with | 0 (Arith::Unchecked, should NOT OSR exit) =====

// In Arith::Unchecked mode, division by zero should return 0 (via chillMod)
// instead of triggering an OSR exit.
function modInt52DivByZeroBitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52DivByZeroBitOr);

for (let i = 0; i < testLoopCount; i++) {
    // Alternate between normal mod and division by zero to stress the JIT.
    if (i % 2 === 0) {
        let result = modInt52DivByZeroBitOr(2147483648, 7);
        if (result !== (2147483648 % 7))
            throw "FAIL: modInt52DivByZeroBitOr normal case, got " + result;
    } else {
        // x % 0 in JS produces NaN, NaN | 0 = 0.
        // With Arith::Unchecked, chillMod returns 0 for zero denominator,
        // then | 0 produces 0. Same result.
        let result = modInt52DivByZeroBitOr(2147483648, 0);
        if (result !== 0)
            throw "FAIL: modInt52DivByZeroBitOr div-by-zero, got " + result + " expected 0";
    }
}

// Pure division by zero case: ensure it doesn't trap or OSR exit excessively.
function modInt52AlwaysZeroDivisorBitOr(a) {
    return (a % 0) | 0;
}
noInline(modInt52AlwaysZeroDivisorBitOr);

for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52AlwaysZeroDivisorBitOr(2147483648 + i);
    if (result !== 0)
        throw "FAIL: modInt52AlwaysZeroDivisorBitOr, got " + result + " expected 0";
}

// ===== Negative zero with | 0 (Arith::Unchecked, -0 | 0 = 0) =====

// When the result is -0, | 0 converts it to +0.
// In Arith::Unchecked mode, we don't need to check for negative zero at all.
function modInt52NegZeroBitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52NegZeroBitOr);

for (let i = 0; i < testLoopCount; i++) {
    // -2^31 % 2^31 would be -0, but | 0 makes it +0 = 0.
    let result = modInt52NegZeroBitOr(-2147483648, 2147483648);
    if (result !== 0)
        throw "FAIL: modInt52NegZeroBitOr, got " + result + " expected 0";
}

// Larger Int52 negative zero case
for (let i = 0; i < testLoopCount; i++) {
    // -2^40 % 2^40 would be -0, but | 0 makes it 0.
    let result = modInt52NegZeroBitOr(-1099511627776, 1099511627776);
    if (result !== 0)
        throw "FAIL: modInt52NegZeroBitOr large, got " + result + " expected 0";
}

// ===== Int52 mod with other bitwise operators (also Arith::Unchecked) =====

function modInt52BitAnd(a, b) {
    return (a % b) & 0x7FFFFFFF;
}
noInline(modInt52BitAnd);

for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitAnd(4503599627370495, 97);
    let expected = (4503599627370495 % 97) & 0x7FFFFFFF;
    if (result !== expected)
        throw "FAIL: modInt52BitAnd, got " + result + " expected " + expected;
}

function modInt52BitXor(a, b) {
    return (a % b) ^ 0;
}
noInline(modInt52BitXor);

for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitXor(2147483648, 7);
    let expected = (2147483648 % 7) ^ 0;
    if (result !== expected)
        throw "FAIL: modInt52BitXor, got " + result + " expected " + expected;
}

function modInt52BitShift(a, b) {
    return (a % b) >> 0;
}
noInline(modInt52BitShift);

for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52BitShift(4503599627370495, 1000000007);
    let expected = (4503599627370495 % 1000000007) >> 0;
    if (result !== expected)
        throw "FAIL: modInt52BitShift, got " + result + " expected " + expected;
}

// ===== Int52 mod with LogicalNot (clears NeedsNaNOrInfinity for Mod/Div) =====

// LogicalNot clears NodeBytecodeNeedsNaNOrInfinity when its child is ArithMod/ArithDiv.
// This is because Mod/Div from integer inputs only produce NaN (never Infinity),
// and ToBoolean(NaN) = false = ToBoolean(0), so chillMod returning 0 for NaN is correct.
function modInt52LogicalNot(a, b) {
    return !(a % b);
}
noInline(modInt52LogicalNot);

// Non-zero remainder: !(nonzero) = false
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52LogicalNot(2147483648, 7);
    if (result !== false)
        throw "FAIL: modInt52LogicalNot non-zero, got " + result + " expected false";
}

// Zero remainder: !(0) = true
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52LogicalNot(2147483648, 2147483648);
    if (result !== true)
        throw "FAIL: modInt52LogicalNot zero remainder, got " + result + " expected true";
}

// Division by zero: NaN case, chillMod returns 0, !(0) = true.
// In JS: x % 0 = NaN, !NaN = true. So the result should be true.
function modInt52LogicalNotDivByZero(a, b) {
    return !(a % b);
}
noInline(modInt52LogicalNotDivByZero);

for (let i = 0; i < testLoopCount; i++) {
    if (i % 2 === 0) {
        let result = modInt52LogicalNotDivByZero(2147483648, 7);
        if (result !== false)
            throw "FAIL: modInt52LogicalNotDivByZero normal, got " + result + " expected false";
    } else {
        let result = modInt52LogicalNotDivByZero(2147483648, 0);
        if (result !== true)
            throw "FAIL: modInt52LogicalNotDivByZero div-by-zero, got " + result + " expected true";
    }
}

// Negative zero with LogicalNot: !(-0) = true, !(+0) = true.
// Both are correctly handled regardless of negative zero optimization.
for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52LogicalNot(-1099511627776, 1099511627776);
    if (result !== true)
        throw "FAIL: modInt52LogicalNot neg zero, got " + result + " expected true";
}

// ===== Combined: Int52 mod in arithmetic then bitwise (NeedsNaNOrInfinity propagation) =====

// (a % b + c) | 0
// ArithAdd propagates NeedsNaNOrInfinity to its children (ArithMod),
// but | 0 clears NeedsNaNOrInfinity. So ArithMod still gets Unchecked if ArithAdd also doesn't need it.
function modInt52AddBitOr(a, b, c) {
    return ((a % b) + c) | 0;
}
noInline(modInt52AddBitOr);

for (let i = 0; i < testLoopCount; i++) {
    let result = modInt52AddBitOr(4503599627370495, 97, 42);
    let expected = ((4503599627370495 % 97) + 42) | 0;
    if (result !== expected)
        throw "FAIL: modInt52AddBitOr, got " + result + " expected " + expected;
}

// ===== Varying inputs to prevent constant folding =====

function modInt52VaryingBitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52VaryingBitOr);

for (let i = 0; i < testLoopCount; i++) {
    let a = 2147483648 + i;
    let b = 7 + (i % 13);
    let result = modInt52VaryingBitOr(a, b);
    let expected = (a % b) | 0;
    if (result !== expected)
        throw "FAIL: modInt52VaryingBitOr at i=" + i + ", got " + result + " expected " + expected;
}

// Varying inputs with occasional zero divisor
function modInt52VaryingZeroBitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52VaryingZeroBitOr);

for (let i = 0; i < testLoopCount; i++) {
    let a = 2147483648 + i;
    // Every 1000th iteration, divisor is 0
    let b = (i % 1000 === 0) ? 0 : (7 + (i % 13));
    let result = modInt52VaryingZeroBitOr(a, b);
    let expected = (a % b) | 0;
    if (result !== expected)
        throw "FAIL: modInt52VaryingZeroBitOr at i=" + i + ", got " + result + " expected " + expected;
}

// ===== CheckOverflow mode (NeedsNaNOrInfinity cleared, but NegZero needed) =====
// When LogicalNot is NOT present and only NeedsNaNOrInfinity is cleared but NegZero is still needed,
// we get Arith::CheckOverflow (not Unchecked).
// This happens when the child is Mod/Div under LogicalNot but negative zero is still observable
// through other uses. This is a more complex scenario.

// ===== Stress test: many different Int52 values through Unchecked path =====

function modInt52StressBitOr(a, b) {
    return (a % b) | 0;
}
noInline(modInt52StressBitOr);

let int52Values = [
    2147483648, -2147483649, 4294967296, -4294967296,
    1099511627776, -1099511627776, 4503599627370495, -4503599627370496,
    2251799813685247, -2251799813685248, 8589934592, -8589934592
];

let divisors = [1, -1, 2, -2, 7, -7, 97, 1000000007, 2147483647, 2147483648];

for (let i = 0; i < testLoopCount; i++) {
    let a = int52Values[i % int52Values.length];
    let b = divisors[i % divisors.length];
    let result = modInt52StressBitOr(a, b);
    let expected = (a % b) | 0;
    if (result !== expected)
        throw "FAIL: modInt52StressBitOr a=" + a + " b=" + b + ", got " + result + " expected " + expected;
}
