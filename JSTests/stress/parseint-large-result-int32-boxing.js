function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

function parseIntHex(string) {
    return parseInt(string, 16);
}
noInline(parseIntHex);

function parseIntNoRadix(string) {
    return parseInt(string);
}
noInline(parseIntNoRadix);

for (let i = 0; i < testLoopCount; ++i) {
    shouldBe(parseIntHex("7fffffff"), 0x7fffffff);
    shouldBe(parseIntNoRadix("123"), 123);
}

shouldBe(parseIntHex("80000000"), 2147483648);
shouldBe(parseIntHex("ffffffff"), 4294967295);
shouldBe(parseIntHex("100000000"), 4294967296);
shouldBe(parseIntHex("-80000000"), -2147483648);
shouldBe(parseIntHex("-80000001"), -2147483649);
shouldBe(parseIntNoRadix("2147483648"), 2147483648);
shouldBe(parseIntNoRadix("4294967296"), 4294967296);
shouldBe(parseIntNoRadix("-2147483649"), -2147483649);
shouldBe(1 / parseIntNoRadix("-0"), -Infinity);
shouldBe(1 / parseIntHex("-0"), -Infinity);
