function shouldBe(actual, expected, msg) {
    if (!Object.is(actual, expected))
        throw new Error("FAIL " + msg + ": expected " + expected + " but got " + actual);
}

function test() {
    shouldBe(parseInt("0", 10), 0, "0");
    shouldBe(parseInt("-0", 10), -0, "-0");
    shouldBe(parseInt("+0", 10), 0, "+0");
    shouldBe(parseInt("7", 10), 7, "7");
    shouldBe(parseInt("42", 10), 42, "42");
    shouldBe(parseInt("007", 10), 7, "007");
    shouldBe(parseInt("12345", 10), 12345, "12345");
    shouldBe(parseInt("-12345", 10), -12345, "-12345");
    shouldBe(parseInt("+12345", 10), 12345, "+12345");
    shouldBe(parseInt("   42", 10), 42, "leading ws");
    shouldBe(parseInt("42   ", 10), 42, "trailing ws");
    shouldBe(parseInt("   42   ", 10), 42, "both ws");
    shouldBe(parseInt("123abc", 10), 123, "123abc");
    shouldBe(parseInt("123.456", 10), 123, "123.456");
    shouldBe(parseInt("999999999", 10), 999999999, "9 digits");
    shouldBe(parseInt("1000000000", 10), 1000000000, "10 digits small");
    shouldBe(parseInt("9999999999", 10), 9999999999, "10 digits");
    shouldBe(parseInt("2147483647", 10), 2147483647, "INT32_MAX");
    shouldBe(parseInt("2147483648", 10), 2147483648, "INT32_MAX+1");
    shouldBe(parseInt("-2147483648", 10), -2147483648, "INT32_MIN");
    shouldBe(parseInt("-2147483649", 10), -2147483649, "INT32_MIN-1");
    shouldBe(parseInt("9007199254740992", 10), 9007199254740992, "2^53");
    shouldBe(parseInt("9007199254740993", 10), 9007199254740992, "2^53+1 rounds");
    shouldBe(parseInt("100000000000000000000000", 10), 1e23, "1e23");
    shouldBe(Number.isNaN(parseInt("", 10)), true, "empty");
    shouldBe(Number.isNaN(parseInt("   ", 10)), true, "ws only");
    shouldBe(Number.isNaN(parseInt("abc", 10)), true, "abc");
    shouldBe(Number.isNaN(parseInt("-", 10)), true, "minus only");
    shouldBe(Number.isNaN(parseInt("- 1", 10)), true, "minus space");
    shouldBe(parseInt("0x10", 10), 0, "0x10 radix10");
    shouldBe(parseInt("0x10"), 16, "0x10 no radix");
    shouldBe(parseInt("0"), 0, "0 no radix");
    shouldBe(parseInt("0", 0), 0, "0 radix 0");
    shouldBe(parseInt("12345"), 12345, "no radix");
    shouldBe(parseInt("12345", 0), 12345, "radix 0");
    shouldBe(parseInt("12345", undefined), 12345, "radix undefined");
    shouldBe(parseInt("  42 ", 10), 42, "unicode ws");
    shouldBe(Number.isNaN(parseInt("１２", 10)), true, "fullwidth digits");
    shouldBe(parseInt("999999999abc", 10), 999999999, "9 digits + junk");
    shouldBe(parseInt("9999999999abc", 10), 9999999999, "10 digits + junk");
    shouldBe(parseInt("000000000000000000000001", 10), 1, "many leading zeros");
}
noInline(test);

for (let i = 0; i < testLoopCount; ++i)
    test();
