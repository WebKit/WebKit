function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error("FAIL: " + msg + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

// Non-Latin1 character to force 16-bit string representation.
var prefix = "\u0100";

// Backreference inside (?i:...) modifier with 16-bit string input.
// The backreference should match case-insensitively.
var re = /(a)(?i:\1)/;

var result = re.exec(prefix + "aA");
shouldBe(result[0], "aA", "/(a)(?i:\\1)/ with 16-bit input 'aA'");

result = re.exec(prefix + "aa");
shouldBe(result[0], "aa", "/(a)(?i:\\1)/ with 16-bit input 'aa'");

result = re.exec(prefix + "ab");
shouldBe(result, null, "/(a)(?i:\\1)/ with 16-bit input 'ab'");

// Non-Latin1 characters in the capture group itself.
var re2 = /(\u0100)(?i:\1)/;
result = re2.exec("\u0100\u0100");
shouldBe(result[0], "\u0100\u0100", "/(\\u0100)(?i:\\1)/ matching identical chars");
