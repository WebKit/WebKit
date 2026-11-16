function testMod2(n) {
    n |= 0; // Ensure int32
    return n % 2;
}
noInline(testMod2);

function testMod4(n) {
    n |= 0;
    return n % 4;
}
noInline(testMod4);

function testMod8(n) {
    n |= 0;
    return n % 8;
}
noInline(testMod8);

function testMod16(n) {
    n |= 0;
    return n % 16;
}
noInline(testMod16);

function testMod32(n) {
    n |= 0;
    return n % 32;
}
noInline(testMod32);

function testMod64(n) {
    n |= 0;
    return n % 64;
}
noInline(testMod64);

function testMod128(n) {
    n |= 0;
    return n % 128;
}
noInline(testMod128);

function testMod256(n) {
    n |= 0;
    return n % 256;
}
noInline(testMod256);

function testMod512(n) {
    n |= 0;
    return n % 512;
}
noInline(testMod512);

function testMod1024(n) {
    n |= 0;
    return n % 1024;
}
noInline(testMod1024);

function testMod2048(n) {
    n |= 0;
    return n % 2048;
}
noInline(testMod2048);

function testMod4096(n) {
    n |= 0;
    return n % 4096;
}
noInline(testMod4096);

function testMod65536(n) {
    n |= 0;
    return n % 65536;
}
noInline(testMod65536);

function testMod1048576(n) {
    n |= 0;
    return n % 1048576;
}
noInline(testMod1048576);

// Test values covering important edge cases
const INT32_MIN = -2147483648;
const INT32_MAX = 2147483647;

const testValues = [
    // Edge cases
    INT32_MIN,
    INT32_MIN + 1,
    INT32_MIN + 255,
    INT32_MIN + 256,
    INT32_MAX,
    INT32_MAX - 1,
    INT32_MAX - 255,
    INT32_MAX - 256,

    // Zero and near zero
    0,
    -1,
    1,

    // Small negative numbers
    -2, -3, -4, -5, -7, -8, -15, -16, -17, -31, -32, -33,
    -63, -64, -65, -127, -128, -129, -255, -256, -257,
    -511, -512, -513, -1023, -1024, -1025,

    // Small positive numbers
    2, 3, 4, 5, 7, 8, 15, 16, 17, 31, 32, 33,
    63, 64, 65, 127, 128, 129, 255, 256, 257,
    511, 512, 513, 1023, 1024, 1025,

    // Medium numbers
    -1000, -10000, -100000, -1000000,
    1000, 10000, 100000, 1000000,

    // Large numbers (close to boundaries)
    -1073741824, // -2^30
    1073741824,  // 2^30
    -536870912,  // -2^29
    536870912,   // 2^29
];

// Expected results for mod 256 (manual verification)
const expectedMod256 = {
    [INT32_MIN]: 0,
    [INT32_MIN + 1]: -255,
    [INT32_MIN + 255]: -1,
    [INT32_MIN + 256]: 0,
    [INT32_MAX]: 255,
    [INT32_MAX - 1]: 254,
    [INT32_MAX - 255]: 0,
    [INT32_MAX - 256]: 255,
    0: 0,
    [-1]: -1,
    1: 1,
    [-5]: -5,
    5: 5,
    [-128]: -128,
    128: 128,
    [-255]: -255,
    255: 255,
    [-256]: 0,
    256: 0,
    [-257]: -1,
    257: 1,
};

// Warm up functions to trigger JIT compilation
for (let i = 0; i < testLoopCount; ++i) {
    testMod2(i);
    testMod4(i);
    testMod8(i);
    testMod16(i);
    testMod32(i);
    testMod64(i);
    testMod128(i);
    testMod256(i);
    testMod512(i);
    testMod1024(i);
    testMod2048(i);
    testMod4096(i);
    testMod65536(i);
    testMod1048576(i);
}

// Test with edge case values
for (let value of testValues) {
    let result;

    // Test mod 2
    result = testMod2(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod2(${value}) returned non-number or NaN: ${result}`);
    let expected2 = value % 2;
    if (result !== expected2)
        throw new Error(`testMod2(${value}) returned ${result}, expected ${expected2}`);

    // Test mod 4
    result = testMod4(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod4(${value}) returned non-number or NaN: ${result}`);
    let expected4 = value % 4;
    if (result !== expected4)
        throw new Error(`testMod4(${value}) returned ${result}, expected ${expected4}`);

    // Test mod 8
    result = testMod8(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod8(${value}) returned non-number or NaN: ${result}`);
    let expected8 = value % 8;
    if (result !== expected8)
        throw new Error(`testMod8(${value}) returned ${result}, expected ${expected8}`);

    // Test mod 16
    result = testMod16(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod16(${value}) returned non-number or NaN: ${result}`);
    let expected16 = value % 16;
    if (result !== expected16)
        throw new Error(`testMod16(${value}) returned ${result}, expected ${expected16}`);

    // Test mod 32
    result = testMod32(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod32(${value}) returned non-number or NaN: ${result}`);
    let expected32 = value % 32;
    if (result !== expected32)
        throw new Error(`testMod32(${value}) returned ${result}, expected ${expected32}`);

    // Test mod 64
    result = testMod64(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod64(${value}) returned non-number or NaN: ${result}`);
    let expected64 = value % 64;
    if (result !== expected64)
        throw new Error(`testMod64(${value}) returned ${result}, expected ${expected64}`);

    // Test mod 128
    result = testMod128(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod128(${value}) returned non-number or NaN: ${result}`);
    let expected128 = value % 128;
    if (result !== expected128)
        throw new Error(`testMod128(${value}) returned ${result}, expected ${expected128}`);

    // Test mod 256 (most important case)
    result = testMod256(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod256(${value}) returned non-number or NaN: ${result}`);
    let expected256 = value % 256;
    if (result !== expected256)
        throw new Error(`testMod256(${value}) returned ${result}, expected ${expected256}`);

    // Verify against manual expected values if available
    if (value in expectedMod256) {
        if (result !== expectedMod256[value])
            throw new Error(`testMod256(${value}) returned ${result}, manually expected ${expectedMod256[value]}`);
    }

    // Test mod 512
    result = testMod512(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod512(${value}) returned non-number or NaN: ${result}`);
    let expected512 = value % 512;
    if (result !== expected512)
        throw new Error(`testMod512(${value}) returned ${result}, expected ${expected512}`);

    // Test mod 1024
    result = testMod1024(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod1024(${value}) returned non-number or NaN: ${result}`);
    let expected1024 = value % 1024;
    if (result !== expected1024)
        throw new Error(`testMod1024(${value}) returned ${result}, expected ${expected1024}`);

    // Test mod 2048
    result = testMod2048(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod2048(${value}) returned non-number or NaN: ${result}`);
    let expected2048 = value % 2048;
    if (result !== expected2048)
        throw new Error(`testMod2048(${value}) returned ${result}, expected ${expected2048}`);

    // Test mod 4096
    result = testMod4096(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod4096(${value}) returned non-number or NaN: ${result}`);
    let expected4096 = value % 4096;
    if (result !== expected4096)
        throw new Error(`testMod4096(${value}) returned ${result}, expected ${expected4096}`);

    // Test mod 65536
    result = testMod65536(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod65536(${value}) returned non-number or NaN: ${result}`);
    let expected65536 = value % 65536;
    if (result !== expected65536)
        throw new Error(`testMod65536(${value}) returned ${result}, expected ${expected65536}`);

    // Test mod 1048576
    result = testMod1048576(value);
    if (typeof result !== 'number' || isNaN(result))
        throw new Error(`testMod1048576(${value}) returned non-number or NaN: ${result}`);
    let expected1048576 = value % 1048576;
    if (result !== expected1048576)
        throw new Error(`testMod1048576(${value}) returned ${result}, expected ${expected1048576}`);
}

// Additional test: Verify results stay in expected range
for (let i = -10000; i < 10000; i++) {
    let result = testMod256(i);

    // Result must be in range [-255, 255]
    if (result < -255 || result > 255)
        throw new Error(`testMod256(${i}) returned ${result} which is outside valid range [-255, 255]`);

    // Result must have same sign as dividend (or be zero)
    if (i > 0 && result < 0)
        throw new Error(`testMod256(${i}) returned negative ${result} for positive input`);
    if (i < 0 && result > 0)
        throw new Error(`testMod256(${i}) returned positive ${result} for negative input`);

    // Verify the result is an int32 (not a double)
    if ((result | 0) !== result)
        throw new Error(`testMod256(${i}) returned ${result} which is not an int32`);
}

// Test that INT32_MIN % power-of-two doesn't cause issues
let intMinMod2 = testMod2(INT32_MIN);
if (intMinMod2 !== 0)
    throw new Error(`INT32_MIN % 2 should be 0, got ${intMinMod2}`);

let intMinMod256 = testMod256(INT32_MIN);
if (intMinMod256 !== 0)
    throw new Error(`INT32_MIN % 256 should be 0, got ${intMinMod256}`);

let intMinMod1024 = testMod1024(INT32_MIN);
if (intMinMod1024 !== 0)
    throw new Error(`INT32_MIN % 1024 should be 0, got ${intMinMod1024}`);

// Test chained modulo operations (strength reduction should handle this)
function chainedMod(n) {
    n |= 0;
    let a = n % 256;
    let b = a % 16;
    let c = b % 4;
    return c;
}
noInline(chainedMod);

for (let i = 0; i < testLoopCount; ++i) {
    chainedMod(i);
}

for (let value of testValues) {
    let result = chainedMod(value);
    let expected = ((value % 256) % 16) % 4;
    if (result !== expected)
        throw new Error(`chainedMod(${value}) returned ${result}, expected ${expected}`);
}
