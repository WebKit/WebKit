// Tests OSR exit from the DoubleRepUse ArithSign path when an object sneaks in.

function shouldBe(actual, expected, message) {
    if (!Object.is(actual, expected))
        throw new Error(message + ": expected " + expected + ", got " + actual);
}

function test(value) {
    return Math.sign(value);
}
noInline(test);

// Warm up with double inputs to lock in DoubleRepUse speculation.
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(1.5), 1, "test(1.5)");
    shouldBe(test(-1.5), -1, "test(-1.5)");
    shouldBe(test(0.0), 0, "test(0.0)");
    shouldBe(test(-0.0), -0, "test(-0.0)");
    shouldBe(test(NaN), NaN, "test(NaN)");
}

// Trigger OSR exit by passing an object with valueOf.
let count = 0;
shouldBe(test({ valueOf() { count++; return 7; } }), 1, "test(object)");
shouldBe(count, 1, "valueOf called once");
shouldBe(test({ valueOf() { count++; return -7; } }), -1, "test(neg object)");
shouldBe(count, 2, "valueOf called twice");
shouldBe(test("3"), 1, "test(string)");
shouldBe(test(undefined), NaN, "test(undefined)");

// Recompile and verify both paths still work.
for (var i = 0; i < 1e5; ++i) {
    shouldBe(test(0.25), 1, "test(0.25) recompiled");
    shouldBe(test(-0.25), -1, "test(-0.25) recompiled");
    shouldBe(test({ valueOf() { return -5; } }), -1, "test(object) recompiled");
}
