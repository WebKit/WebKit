function shouldBe(actual, expected, what) {
    if (actual !== expected)
        throw new Error(what + ": bad value " + actual + ", expected " + expected);
}

// A right shift by a constant bounds its result only by the count modulo 32, so x >> 33 is
// x >> 1 and x >>> 32 is x >>> 0. Multiplying such a value by a large int32 gives a product
// above 2^53, where the double result of the multiply and the low 32 bits of the exact product
// differ after | 0.

function shiftBy33ThenMultiply(x, y) { return ((x >> 33) * y) | 0; }
noInline(shiftBy33ThenMultiply);

function unsignedShiftBy32ThenMultiply(x, y) { return ((x >>> 32) * y) | 0; }
noInline(unsignedShiftBy32ThenMultiply);

function shiftBy10ThenMultiply(x, y) { return ((x >> 10) * y) | 0; }
noInline(shiftBy10ThenMultiply);

function unsignedShiftBy10ThenMultiply(x, y) { return ((x >>> 10) * y) | 0; }
noInline(unsignedShiftBy10ThenMultiply);

const large = 0x7fffffff;
for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(shiftBy33ThenMultiply(i, 3), (i >> 1) * 3, "warm shiftBy33ThenMultiply");
    shouldBe(unsignedShiftBy32ThenMultiply(i, 3), i * 3, "warm unsignedShiftBy32ThenMultiply");
    shouldBe(shiftBy10ThenMultiply(i, 3), (i >> 10) * 3, "warm shiftBy10ThenMultiply");
    shouldBe(unsignedShiftBy10ThenMultiply(i, 3), (i >>> 10) * 3, "warm unsignedShiftBy10ThenMultiply");
}

shouldBe(shiftBy33ThenMultiply(large, large), ((large >> 1) * large) | 0, "shiftBy33ThenMultiply(large, large)");
shouldBe(shiftBy33ThenMultiply(-0x80000000, large), (-0x40000000 * large) | 0, "shiftBy33ThenMultiply(-2^31, large)");
shouldBe(unsignedShiftBy32ThenMultiply(-1, large), (0xffffffff * large) | 0, "unsignedShiftBy32ThenMultiply(-1, large)");
shouldBe(unsignedShiftBy32ThenMultiply(-3, -large), (0xfffffffd * -large) | 0, "unsignedShiftBy32ThenMultiply(-3, -large)");
shouldBe(shiftBy10ThenMultiply(large, large), ((large >> 10) * large) | 0, "shiftBy10ThenMultiply(large, large)");
shouldBe(shiftBy10ThenMultiply(-0x80000000, -0x80000000), ((-0x80000000 >> 10) * -0x80000000) | 0, "shiftBy10ThenMultiply(-2^31, -2^31)");
shouldBe(unsignedShiftBy10ThenMultiply(-1, large), ((0xffffffff >>> 10) * large) | 0, "unsignedShiftBy10ThenMultiply(-1, large)");
