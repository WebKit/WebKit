// RegExp.prototype.test on a constant RegExp becomes RegExpTestInline in DFG/FTL. The
// inlined Yarr code now covers u/v-flag patterns and patterns whose terms need frame
// slots for backtracking (quantifiers, alternations, .-classes in unicode mode).

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

const inputs = [
    "", "hello world", "world", "hello worXd", "woooorld", "abc", "ABC", "K", "k", "\u212a", "\u017f", "s", "S",
    "xxxyz", "xyz", "z", "yz", "ababe", "cde", "abcde", "aab", "aaaaab", "  #x", "#", "\t#",
    "\u{1F600}", "a\u{1F600}b", "\ud83d", "\ude00", "\u00e9", "foo", "a foo b", "foobar", "bar", "ab", "ba",
    "a\nb", "\n", "1", "12 ", " 12", "x", "xy", "xxy", "qqq", "aq", "0", "Z", "zebra", "aeiou",
    "hello w\u00f6rld", "hello " + "wor" + "ld",
];

// [regexp, expected result for each input above]
const cases = [
    [/world/u,           "0110000000000000000000000000000000000000000000000001"],
    [/wor.d/u,           "0111000000000000000000000000000000000000000000000001"],
    [/wor.d/v,           "0111000000000000000000000000000000000000000000000001"],
    [/wo+rld/,           "0110100000000000000000000000000000000000000000000001"],
    [/wo+rld/u,          "0110100000000000000000000000000000000000000000000001"],
    [/x*y?z/u,           "0000000000000111100000000000000000000000000000001000"],
    [/a{2,4}b/u,         "0000000000000000000011000000000000000000000000000000"],
    [/^\s*#/u,           "0000000000000000000000111000000000000000000000000000"],
    [/[k-s]/iu,          "0111100111111000000000000000001111000000000011001111"],
    [/\p{Lu}/u,          "0001001101001000000000000000000000000000000000010000"],
    [/[\p{L}--[a-z]]/v,  "0001001101101000000000000000010000000000000000010010"],
    [/[^\x00-\x7f]/u,    "0000000001100000000000000111110000000000000000000010"],
    [/\u{1F600}/u,       "0000000000000000000000000110000000000000000000000000"],
    [/./su,              "0111111111111111111111111111111111111111111111111111"],
    [/^.$/u,             "0000000111111001000000010101110000000010010000110000"],
    [/foo|bar/u,         "0000000000000000000000000000001111000000000000000000"],
    [/\D/u,              "0111111111111111111111111111111111111101111111011111"],
    [/\W+/u,             "0101000001100000000000111111110100001101100000000011"],
    [/q*$/u,             "1111111111111111111111111111111111111111111111111111"],
    [/^/mu,              "1111111111111111111111111111111111111111111111111111"],
    [/(?:ab|cd)+e/u,     "0000000000000000011100000000000000000000000000000000"],
    [/[a-c]+|[x-z]{2}/u, "0000010000000110111111000010000111111000001101001100"],
];

for (const [re, expected] of cases)
    shouldBe(expected.length, inputs.length, re + " expectation length");

function makeTest(re) {
    // A RegExp literal inside the function is a NewRegExp node that the strength reduction
    // phase can turn into RegExpTestInline.
    const test = new Function("s", "return " + re + ".test(s);");
    noInline(test);
    return test;
}

for (const [re, expected] of cases) {
    const test = makeTest(re);
    for (let i = 0; i < testLoopCount; ++i) {
        const s = inputs[i % inputs.length];
        shouldBe(test(s), expected[i % inputs.length] === "1", re + ".test(" + JSON.stringify(s) + ")");
    }
}
