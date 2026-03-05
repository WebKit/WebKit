// Test Int52 modulo operation with comprehensive edge cases
function moduloInt52(a, b) {
    return a % b;
}
noInline(moduloInt52);

// Int52 values (exceed Int32 max of 2147483647)
let large = 2147483648;  // 2^31
let expected = large % 7;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloInt52(large, 7);
    if (result !== expected)
        throw "FAIL: int52 modulo basic case, got " + result + " expected " + expected;
}

// Larger Int52 values
let larger = 4503599627370496;  // 2^52
let largerExpected = larger % 1000000007;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloInt52(larger, 1000000007);
    if (result !== largerExpected)
        throw "FAIL: int52 modulo large case, got " + result + " expected " + largerExpected;
}

// Negative Int52
let negative = -2147483649;
let negExpected = negative % 10;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloInt52(negative, 10);
    if (result !== negExpected)
        throw "FAIL: int52 modulo negative numerator, got " + result + " expected " + negExpected;
}

// Test with negative divisor
let negDivisorExpected = large % -7;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloInt52(large, -7);
    if (result !== negDivisorExpected)
        throw "FAIL: int52 modulo negative divisor, got " + result + " expected " + negDivisorExpected;
}

// Test both negative
let bothNegExpected = negative % -7;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloInt52(negative, -7);
    if (result !== bothNegExpected)
        throw "FAIL: int52 modulo both negative, got " + result + " expected " + bothNegExpected;
}

// Test AnyIntAsDouble case - values stored as doubles but represent integers
// This tests the modShouldSpeculateInt52 logic for AnyIntAsDouble prediction.
function moduloAnyIntAsDouble(a, b) {
    // Force values to be doubles by doing floating point operations
    // that result in integer values (AnyIntAsDouble prediction).
    return (a * 1.0) % (b * 1.0);
}
noInline(moduloAnyIntAsDouble);

// Test with Int52-range values that come from double operations
let doubleInt52 = 2147483648.0;  // 2^31 as double
let doubleExpected = doubleInt52 % 7;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloAnyIntAsDouble(doubleInt52, 7);
    if (result !== doubleExpected)
        throw "FAIL: AnyIntAsDouble modulo case, got " + result + " expected " + doubleExpected;
}

// Test with computed Int52 values that start as doubles
function moduloComputedInt52(x) {
    // x will be a double, but represents an integer
    let largeValue = x * 2147483648;  // Will be Int52-range
    return largeValue % 1000000007;
}
noInline(moduloComputedInt52);

let computedExpected = (2 * 2147483648) % 1000000007;
for (let i = 0; i < testLoopCount; i++) {
    let result = moduloComputedInt52(2);
    if (result !== computedExpected)
        throw "FAIL: computed Int52 modulo case, got " + result + " expected " + computedExpected;
}

// ===== Negative Zero Tests =====
// In JavaScript, -X % Y where X is divisible by Y produces -0

function checkNegativeZero(value, testName) {
    if (value !== 0)
        throw "FAIL: " + testName + " should be zero, got " + value;
    if (1 / value !== -Infinity)
        throw "FAIL: " + testName + " should be -0, got " + (1/value);
}

function checkPositiveZero(value, testName) {
    if (value !== 0)
        throw "FAIL: " + testName + " should be zero, got " + value;
    if (1 / value !== Infinity)
        throw "FAIL: " + testName + " should be +0, got " + (1/value);
}

// Test -0 with Int52 values (avoid constant folding with arguments)
function modNegZero1(a, b) {
    return a % b;
}
noInline(modNegZero1);

for (let i = 0; i < testLoopCount; i++) {
    let result = modNegZero1(-2147483648, 2147483648);  // Int52 boundary: -2^31 % 2^31 = -0
    checkNegativeZero(result, "modNegZero1: -2147483648 % 2147483648");
}

// More -0 cases with different Int52 values
function modNegZero2(a, b) {
    return a % b;
}
noInline(modNegZero2);

for (let i = 0; i < testLoopCount; i++) {
    let result = modNegZero2(-4294967296, 2147483648);  // -2^32 % 2^31 = -0
    checkNegativeZero(result, "modNegZero2: -4294967296 % 2147483648");
}

// Test -0 with larger Int52 values
function modNegZero3(a, b) {
    return a % b;
}
noInline(modNegZero3);

for (let i = 0; i < testLoopCount; i++) {
    let result = modNegZero3(-1099511627776, 1099511627776);  // -2^40 % 2^40 = -0
    checkNegativeZero(result, "modNegZero3: -1099511627776 % 1099511627776");
}

// Test -0 with negative divisor produces -0
function modNegZero4(a, b) {
    return a % b;
}
noInline(modNegZero4);

for (let i = 0; i < testLoopCount; i++) {
    let result = modNegZero4(-2147483648, -2147483648);  // -2^31 % -2^31 = -0
    checkNegativeZero(result, "modNegZero4: -2147483648 % -2147483648");
}

// Test +0 with positive numerator
function modPosZero1(a, b) {
    return a % b;
}
noInline(modPosZero1);

for (let i = 0; i < testLoopCount; i++) {
    let result = modPosZero1(2147483648, 2147483648);  // 2^31 % 2^31 = +0
    checkPositiveZero(result, "modPosZero1: 2147483648 % 2147483648");
}

// Test +0 with positive numerator and negative divisor
function modPosZero2(a, b) {
    return a % b;
}
noInline(modPosZero2);

for (let i = 0; i < testLoopCount; i++) {
    let result = modPosZero2(4294967296, -2147483648);  // 2^32 % -2^31 = +0
    checkPositiveZero(result, "modPosZero2: 4294967296 % -2147483648");
}

// ===== Edge Cases with Int52 Boundaries =====

// Test with MIN_INT52 equivalent values
function modMinInt52(a, b) {
    return a % b;
}
noInline(modMinInt52);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMinInt52(-4503599627370496, 1000000007);  // -2^52 % prime
    let expected = -4503599627370496 % 1000000007;
    if (result !== expected)
        throw "FAIL: modMinInt52, got " + result + " expected " + expected;
}

// Test with MAX_INT52 equivalent values
function modMaxInt52(a, b) {
    return a % b;
}
noInline(modMaxInt52);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMaxInt52(4503599627370495, 1000000007);  // 2^52-1 % prime
    let expected = 4503599627370495 % 1000000007;
    if (result !== expected)
        throw "FAIL: modMaxInt52, got " + result + " expected " + expected;
}

// Test modulo by -1 (always produces -0 for negative, +0 for positive)
function modByNegOne(a) {
    return a % -1;
}
noInline(modByNegOne);

for (let i = 0; i < testLoopCount; i++) {
    let result = modByNegOne(-2147483649);
    checkNegativeZero(result, "modByNegOne: -2147483649 % -1");
}

for (let i = 0; i < testLoopCount; i++) {
    let result = modByNegOne(2147483649);
    checkPositiveZero(result, "modByNegOne: 2147483649 % -1");
}

// Test modulo by 1 (always produces +0 for positive, -0 for negative)
function modByOne(a) {
    return a % 1;
}
noInline(modByOne);

for (let i = 0; i < testLoopCount; i++) {
    let result = modByOne(-2147483649);
    checkNegativeZero(result, "modByOne: -2147483649 % 1");
}

for (let i = 0; i < testLoopCount; i++) {
    let result = modByOne(2147483649);
    checkPositiveZero(result, "modByOne: 2147483649 % 1");
}

// ===== Mixed positive/negative with various divisors =====

function modMixed1(a, b) {
    return a % b;
}
noInline(modMixed1);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMixed1(-9007199254740991, 3);  // -(2^53-1) % 3
    let expected = -9007199254740991 % 3;
    if (result !== expected)
        throw "FAIL: modMixed1, got " + result + " expected " + expected;
}

function modMixed2(a, b) {
    return a % b;
}
noInline(modMixed2);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMixed2(9007199254740991, -3);  // (2^53-1) % -3
    let expected = 9007199254740991 % -3;
    if (result !== expected)
        throw "FAIL: modMixed2, got " + result + " expected " + expected;
}

// Test with prime numbers as divisors (common in hash functions)
function modPrime1(a, b) {
    return a % b;
}
noInline(modPrime1);

for (let i = 0; i < testLoopCount; i++) {
    let result = modPrime1(1099511627776, 2147483647);  // 2^40 % (2^31-1)
    let expected = 1099511627776 % 2147483647;
    if (result !== expected)
        throw "FAIL: modPrime1, got " + result + " expected " + expected;
}

// Test with powers of 2 (different optimization path possible)
function modPowerOfTwo(a, b) {
    return a % b;
}
noInline(modPowerOfTwo);

for (let i = 0; i < testLoopCount; i++) {
    let result = modPowerOfTwo(4503599627370495, 1073741824);  // (2^52-1) % 2^30
    let expected = 4503599627370495 % 1073741824;
    if (result !== expected)
        throw "FAIL: modPowerOfTwo, got " + result + " expected " + expected;
}

// Test with small divisor and large Int52 numerator
function modSmallDivisor(a, b) {
    return a % b;
}
noInline(modSmallDivisor);

for (let i = 0; i < testLoopCount; i++) {
    let result = modSmallDivisor(4503599627370495, 97);  // (2^52-1) % 97
    let expected = 4503599627370495 % 97;
    if (result !== expected)
        throw "FAIL: modSmallDivisor, got " + result + " expected " + expected;
}

// Test consecutive Int52 values
function modConsecutive(a, b) {
    return a % b;
}
noInline(modConsecutive);

for (let i = 0; i < testLoopCount; i++) {
    let result = modConsecutive(2147483649, 2147483648);  // (2^31+1) % 2^31 = 1
    let expected = 2147483649 % 2147483648;
    if (result !== expected)
        throw "FAIL: modConsecutive, got " + result + " expected " + expected;
}

// Test with negative zero as intermediate (numerator is negative, result would be zero)
function modIntermediateNegZero(a, b) {
    return a % b;
}
noInline(modIntermediateNegZero);

for (let i = 0; i < testLoopCount; i++) {
    let result = modIntermediateNegZero(-8589934592, 4294967296);  // -2^33 % 2^32 = -0
    checkNegativeZero(result, "modIntermediateNegZero: -8589934592 % 4294967296");
}

// ===== NEW: Additional Edge Cases =====

// Test exactly at Int52 boundaries
function modExactMinInt52(a, b) {
    return a % b;
}
noInline(modExactMinInt52);

for (let i = 0; i < testLoopCount; i++) {
    let result = modExactMinInt52(-2251799813685248, 7);  // MIN_INT52 % 7
    let expected = -2251799813685248 % 7;
    if (result !== expected)
        throw "FAIL: modExactMinInt52, got " + result + " expected " + expected;
}

function modExactMaxInt52(a, b) {
    return a % b;
}
noInline(modExactMaxInt52);

for (let i = 0; i < testLoopCount; i++) {
    let result = modExactMaxInt52(2251799813685247, 7);  // MAX_INT52 % 7
    let expected = 2251799813685247 % 7;
    if (result !== expected)
        throw "FAIL: modExactMaxInt52, got " + result + " expected " + expected;
}

// Test MIN_INT52 % MIN_INT52 = -0
function modMinByMin(a, b) {
    return a % b;
}
noInline(modMinByMin);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMinByMin(-2251799813685248, -2251799813685248);
    checkNegativeZero(result, "modMinByMin: MIN_INT52 % MIN_INT52");
}

// Test MAX_INT52 % MAX_INT52 = +0
function modMaxByMax(a, b) {
    return a % b;
}
noInline(modMaxByMax);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMaxByMax(2251799813685247, 2251799813685247);
    checkPositiveZero(result, "modMaxByMax: MAX_INT52 % MAX_INT52");
}

// Test values just above Int32 MAX to ensure Int52 path is taken
function modJustAboveInt32(a, b) {
    return a % b;
}
noInline(modJustAboveInt32);

for (let i = 0; i < testLoopCount; i++) {
    let result = modJustAboveInt32(2147483648, 1000000007);  // 2^31 % prime
    let expected = 2147483648 % 1000000007;
    if (result !== expected)
        throw "FAIL: modJustAboveInt32, got " + result + " expected " + expected;
}

// Test values just below Int32 MIN to ensure Int52 path is taken
function modJustBelowInt32Min(a, b) {
    return a % b;
}
noInline(modJustBelowInt32Min);

for (let i = 0; i < testLoopCount; i++) {
    let result = modJustBelowInt32Min(-2147483649, 1000000007);  // -(2^31+1) % prime
    let expected = -2147483649 % 1000000007;
    if (result !== expected)
        throw "FAIL: modJustBelowInt32Min, got " + result + " expected " + expected;
}

// Test with Fibonacci-like large numbers
function modFibonacci(a, b) {
    return a % b;
}
noInline(modFibonacci);

for (let i = 0; i < testLoopCount; i++) {
    let result = modFibonacci(2971215073, 1836311903);  // F44 % F43
    let expected = 2971215073 % 1836311903;
    if (result !== expected)
        throw "FAIL: modFibonacci, got " + result + " expected " + expected;
}

// Test modulo that produces maximum negative result
function modMaxNegativeResult(a, b) {
    return a % b;
}
noInline(modMaxNegativeResult);

for (let i = 0; i < testLoopCount; i++) {
    // -2251799813685247 % -2251799813685248 = -2251799813685247 (largest negative remainder)
    let result = modMaxNegativeResult(-2251799813685247, -2251799813685248);
    let expected = -2251799813685247 % -2251799813685248;
    if (result !== expected)
        throw "FAIL: modMaxNegativeResult, got " + result + " expected " + expected;
}

// Test modulo that produces maximum positive result
function modMaxPositiveResult(a, b) {
    return a % b;
}
noInline(modMaxPositiveResult);

for (let i = 0; i < testLoopCount; i++) {
    // 2251799813685247 % 2251799813685248 = 2251799813685247 (largest positive remainder)
    let result = modMaxPositiveResult(2251799813685247, 2251799813685248);
    let expected = 2251799813685247 % 2251799813685248;
    if (result !== expected)
        throw "FAIL: modMaxPositiveResult, got " + result + " expected " + expected;
}

// Test with divisor = 2 (simple pattern, often optimized)
function modByTwo(a) {
    return a % 2;
}
noInline(modByTwo);

for (let i = 0; i < testLoopCount; i++) {
    let result = modByTwo(4503599627370495);  // (2^52-1) % 2 = 1
    if (result !== 1)
        throw "FAIL: modByTwo odd, got " + result;
}

for (let i = 0; i < testLoopCount; i++) {
    let result = modByTwo(4503599627370496);  // 2^52 % 2 = 0
    checkPositiveZero(result, "modByTwo even");
}

// Test with very small negative divisor
function modByNegTwo(a) {
    return a % -2;
}
noInline(modByNegTwo);

for (let i = 0; i < testLoopCount; i++) {
    let result = modByNegTwo(-4503599627370495);  // -(2^52-1) % -2 = -1
    if (result !== -1)
        throw "FAIL: modByNegTwo odd, got " + result;
}

for (let i = 0; i < testLoopCount; i++) {
    let result = modByNegTwo(-4503599627370496);  // -2^52 % -2 = -0
    checkNegativeZero(result, "modByNegTwo even");
}

// Test Mersenne-like numbers (2^n - 1)
function modMersenne(a, b) {
    return a % b;
}
noInline(modMersenne);

for (let i = 0; i < testLoopCount; i++) {
    let result = modMersenne(2199023255551, 2147483647);  // (2^41-1) % (2^31-1)
    let expected = 2199023255551 % 2147483647;
    if (result !== expected)
        throw "FAIL: modMersenne, got " + result + " expected " + expected;
}

// Test with alternating large values
function modAlternating(a, b) {
    return a % b;
}
noInline(modAlternating);

for (let i = 0; i < testLoopCount; i++) {
    let a_val = (i % 2 === 0) ? 1099511627776 : -1099511627776;  // ±2^40
    let result = modAlternating(a_val, 1000000007);
    let expected = a_val % 1000000007;
    if (result !== expected)
        throw "FAIL: modAlternating iteration " + i + ", got " + result + " expected " + expected;
}

// Test pattern that stresses negative zero with varying numerators
function modNegZeroPattern(a, b) {
    return a % b;
}
noInline(modNegZeroPattern);

let divisorsForNegZero = [2147483648, 4294967296, 1099511627776, 2251799813685248];
for (let divisor of divisorsForNegZero) {
    for (let i = 0; i < 100000; i++) {
        let result = modNegZeroPattern(-divisor, divisor);
        checkNegativeZero(result, "modNegZeroPattern: -" + divisor + " % " + divisor);
    }
}

// Test result = divisor - 1 (common case)
function modAlmostDivisor(a, b) {
    return a % b;
}
noInline(modAlmostDivisor);

for (let i = 0; i < testLoopCount; i++) {
    let divisor = 1000000007;
    let result = modAlmostDivisor(2000000013, divisor);  // (2*divisor - 1) % divisor = divisor - 1
    let expected = 1000000006;
    if (result !== expected)
        throw "FAIL: modAlmostDivisor, got " + result + " expected " + expected;
}

function negZeroWithInt32Overflow(x) {
    let sum = x + x;
    let neg = -sum;
    return (neg % sum) | 0;
}
for (let i = 0; i < testLoopCount; i++) {
    let result = negZeroWithInt32Overflow(0x7FFFFFFF);
    let expected = 0;
    if (result !== expected)
        throw "FAIL: negZeroWith32Overflow, got " + result + " expected " + expected;
}