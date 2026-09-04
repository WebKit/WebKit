// Exercises the double->int32 conversion (DoubleAsInt32 / rounding / bit-op coercion) across all
// tiers. On ARM64 with FEAT_JSCVT this is lowered using FJCVTZS plus a flag branch; the negative-zero
// handling is the subtle part: when the bytecode observes -0 it must be preserved (accepted as 0 for
// "ignore -0" sites, and detected for "check -0" sites). Throws on any mismatch; silent on success.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual) + ' expected ' + String(expected));
}
noInline(shouldBe);

function shouldBeNegativeZero(value) {
    if (!Object.is(value, -0))
        throw new Error('expected -0 but got: ' + String(value));
}
noInline(shouldBeNegativeZero);

// Bit-or coercion ignores negative zero: (-0)|0 === 0.
function bitOr(x) { return x | 0; }
noInline(bitOr);

// Typed-array store goes through ToInt32 (DoubleAsInt32-style) and ignores -0.
function viaInt32Array(x) {
    var a = new Int32Array(1);
    a[0] = x;
    return a[0];
}
noInline(viaInt32Array);

// Math.round into an int32 context where -0 must be ignored (used numerically).
function roundIgnoringNegZero(x) { return Math.round(x) | 0; }
noInline(roundIgnoringNegZero);

// Math.round whose -0 result is observed (Object.is): -0 must be preserved.
function roundObservingNegZero(x) { return Math.round(x); }
noInline(roundObservingNegZero);

function test() {
    // Ignore-negative-zero conversions.
    shouldBe(bitOr(-0.0), 0);
    shouldBe(bitOr(0.0), 0);
    shouldBe(bitOr(3.0), 3);
    shouldBe(bitOr(-1.0), -1);
    shouldBe(bitOr(2147483647.0), 2147483647);  // INT32_MAX
    shouldBe(bitOr(-2147483648.0), -2147483648); // INT32_MIN
    shouldBe(bitOr(2147483648.0), -2147483648);  // 2^31 wraps
    shouldBe(bitOr(4294967296.0), 0);            // 2^32 wraps
    shouldBe(bitOr(3000000000.0), -1294967296);  // > INT32_MAX wraps
    shouldBe(bitOr(NaN), 0);
    shouldBe(bitOr(Infinity), 0);
    shouldBe(bitOr(-Infinity), 0);
    shouldBe(bitOr(1.5), 1);
    shouldBe(bitOr(-1.5), -1);

    shouldBe(viaInt32Array(-0.0), 0);
    shouldBe(viaInt32Array(5.0), 5);
    shouldBe(viaInt32Array(-7.0), -7);
    shouldBe(viaInt32Array(2147483648.0), -2147483648);

    shouldBe(roundIgnoringNegZero(0.4), 0);
    shouldBe(roundIgnoringNegZero(-0.4), 0); // Math.round(-0.4) is -0, coerced to 0.
    shouldBe(roundIgnoringNegZero(2.6), 3);

    // Observe-negative-zero conversions: -0 must survive.
    shouldBeNegativeZero(roundObservingNegZero(-0.4));
    shouldBe(roundObservingNegZero(0.4), 0);
    shouldBe(roundObservingNegZero(2.6), 3);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
