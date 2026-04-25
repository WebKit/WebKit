function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error('bad value: ' + actual + ', expected: ' + expected);
}

function testMod(x, y) {
    return x % y;
}
noInline(testMod);

// Test negative zero case: when numerator is negative and remainder is 0
function testNegativeZero() {
    // -0 % 1 should be -0
    shouldBe(testMod(-0, 1), -0);
    shouldBe(testMod(-0, 2), -0);
    shouldBe(testMod(-0, 10), -0);

    // -4 % 2 should be -0 (evenly divisible)
    shouldBe(testMod(-4, 2), -0);
    shouldBe(testMod(-10, 5), -0);
    shouldBe(testMod(-100, 10), -0);
}

// Test overflow case: when (x / y) * y would overflow
function testOverflow() {
    // On ARM64, the multiply check can overflow for large values
    // INT32_MIN % -1 is a special case that should return -0
    shouldBe(testMod(-2147483648, -1), -0);

    // Other cases near overflow boundaries
    shouldBe(testMod(2147483647, 2147483647), 0);
    shouldBe(testMod(-2147483648, 2147483647), -1);
    shouldBe(testMod(2147483647, -2147483648), 2147483647);
}

// Test regular modulo operations to ensure we JIT compile
function testRegularMod() {
    shouldBe(testMod(10, 3), 1);
    shouldBe(testMod(17, 5), 2);
    shouldBe(testMod(-10, 3), -1);
    shouldBe(testMod(-17, 5), -2);
    shouldBe(testMod(10, -3), 1);
    shouldBe(testMod(17, -5), 2);
    shouldBe(testMod(-10, -3), -1);
    shouldBe(testMod(-17, -5), -2);
    shouldBe(testMod(0, -1), 0);
}

// Main test loop - ensures JIT compilation
for (var i = 0; i < testLoopCount; ++i) {
    testRegularMod();
    testNegativeZero();
    testOverflow();
}