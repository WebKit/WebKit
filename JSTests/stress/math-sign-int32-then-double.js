// Tests OSR exit from the Int32Use ArithSign path when a double sneaks in,
// and verifies the result type stays consistent across recompilation.

function shouldBe(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ": expected " + expected + " (" + (1/expected) + "), got " + actual + " (" + (1/actual) + ")");
}

function test(value) {
    return Math.sign(value);
}
noInline(test);

// Warm up with Int32 inputs to lock in Int32Use speculation.
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(1), 1, "test(1)");
    shouldBe(test(-1), -1, "test(-1)");
    shouldBe(test(0), 0, "test(0)");
    shouldBe(test(2147483647), 1, "test(INT_MAX)");
    shouldBe(test(-2147483648), -1, "test(INT_MIN)");
}

// Trigger OSR exit by passing doubles. Verify ±0 and NaN passthrough.
shouldBe(test(0.5), 1, "test(0.5) after Int32 warmup");
shouldBe(test(-0.5), -1, "test(-0.5) after Int32 warmup");
shouldBe(test(-0.0), -0, "test(-0.0) after Int32 warmup");
shouldBe(test(0.0), 0, "test(0.0) after Int32 warmup");
shouldBe(test(NaN), NaN, "test(NaN) after Int32 warmup");
shouldBe(test(Infinity), 1, "test(Infinity) after Int32 warmup");
shouldBe(test(-Infinity), -1, "test(-Infinity) after Int32 warmup");

// Recompile with mixed inputs, confirm both paths still work.
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(2.5), 1, "test(2.5) recompiled");
    shouldBe(test(-2.5), -1, "test(-2.5) recompiled");
    shouldBe(test(-0.0), -0, "test(-0.0) recompiled");
    shouldBe(test(NaN), NaN, "test(NaN) recompiled");
    shouldBe(test(3), 1, "test(3) recompiled");
    shouldBe(test(-3), -1, "test(-3) recompiled");
}
