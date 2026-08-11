function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`FAIL: expected '${expected}' actual '${actual}'`);
}

function notReached() {
    throw new Error("should not reach here");
}

for (const x of [undefined, null, true, 42, [], {}]) {
    try {
        RegExp.escape(x);
        notReached();
    } catch (e) {
        shouldBe(e instanceof TypeError, true);
        shouldBe(e.message, "RegExp.escape requires a string");
    }
}

shouldBe(RegExp.escape("test"), "\\x74est");
shouldBe(RegExp.escape("1234"), "\\x31234");
shouldBe(RegExp.escape("✅unicode✅"), "✅unicode✅");
shouldBe(RegExp.escape("\t"), "\\t");
shouldBe(RegExp.escape("\n"), "\\n");
shouldBe(RegExp.escape("\v"), "\\v");
shouldBe(RegExp.escape("\f"), "\\f");
shouldBe(RegExp.escape("\r"), "\\r");
shouldBe(RegExp.escape("^$\\.*+?()[]{}|/"), "\\^\\$\\\\\\.\\*\\+\\?\\(\\)\\[\\]\\{\\}\\|\\/");
shouldBe(RegExp.escape(",-=<>#&!%:;@~'`\""), "\\x2c\\x2d\\x3d\\x3c\\x3e\\x23\\x26\\x21\\x25\\x3a\\x3b\\x40\\x7e\\x27\\x60\\x22");
shouldBe(RegExp.escape(" \uFEFF\u2000\u3000"), "\\x20\\ufeff\\u2000\\u3000");
shouldBe(RegExp.escape("\uD7FF_\uD800_\uD801_\uDFFE_\uDFFF_\uE000"), "\uD7FF_\\ud800_\\ud801_\\udffe_\\udfff_\uE000");
