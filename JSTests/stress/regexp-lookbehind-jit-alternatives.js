function shouldBe(actual, expected) {
    actual = JSON.stringify(actual, (key, value) => value === undefined ? "<undefined>" : value);
    expected = JSON.stringify(expected, (key, value) => value === undefined ? "<undefined>" : value);
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function matchOf(re, string) {
    let match = re.exec(string);
    return match ? [match.index, ...match] : null;
}

shouldBe(matchOf(/(?<=abc|a|)x/, "x"), [0, "x"]);
shouldBe(matchOf(/(?<=abc|a|)x/, "ax"), [1, "x"]);
shouldBe(matchOf(/(?<=|a)(x)/, "ax"), [1, "x", "x"]);
shouldBe(matchOf(/(?<!abc|a|)x/, "zx"), null);
shouldBe(matchOf(/(?<=abcd|bc)x/, "abcx"), [3, "x"]);
shouldBe(matchOf(/(?<=abcd|bc)x/, "abcdx"), [4, "x"]);
shouldBe(matchOf(/(?<=bc|abcd)x/, "abcdx"), [4, "x"]);
shouldBe(matchOf(/(?<=(abcd|bc))x/, "abcdx"), [4, "x", "abcd"]);
shouldBe(matchOf(/(?<=(bc|abcd))x/, "abcdx"), [4, "x", "abcd"]);
shouldBe(matchOf(/(?<=a(?:bcd|c)|zz)x/, "acx"), [2, "x"]);
shouldBe(matchOf(/(?<=x[ab]c)d/, "xbcd"), [3, "d"]);
shouldBe(matchOf(/(?<=x[ab]c)d/, "ybcd"), null);
shouldBe(matchOf(/(?<=\dx[ab])c/, "1xac"), [3, "c"]);
shouldBe(matchOf(/(?<=(?<=(?<=a)b)c)d/, "abcd"), [3, "d"]);
shouldBe(matchOf(/(?<=(?<=(?<=x)b)c)d/, "abcd"), null);
shouldBe(matchOf(/(?<=(?<!(?<=a)b)c)d/, "abcd"), null);
shouldBe(matchOf(/(?<=(?<!(?<=a)b)c)d/, "xbcd"), [3, "d"]);
shouldBe(matchOf(/(?=(?<=(?=(?<=a)b)b)b)./, "abb"), [2, "b"]);
shouldBe(matchOf(/(?<=a)bcde|(?<=x)yzwv/, "..xyzwv..abcde"), [3, "yzwv"]);
shouldBe(matchOf(/abcd(?<=d)|wxyz(?<!q...)/i, "...WXYZ"), [3, "WXYZ"]);
shouldBe(matchOf(/.*(?<=x)abc.*/, "11xabc22\nxabc"), [0, "11xabc22"]);
shouldBe(matchOf(/.*(?<!x)abc.*/, "11xabc22\nyabc3"), [9, "yabc3"]);
shouldBe(matchOf(/(?<=a)b$/, "ab".repeat(3) + "cab"), [8, "b"]);
shouldBe(matchOf(/(?<=a+)b$/, "b" + "a".repeat(50) + "b"), [51, "b"]);
shouldBe(matchOf(/(?<=^a+)b$/, "a".repeat(50) + "b"), [50, "b"]);
shouldBe(matchOf(/(?<=^a+)b$/, "ba".repeat(25) + "b"), null);
shouldBe(matchOf(/^(?<=)abc/, "abc"), [0, "abc"]);
shouldBe(matchOf(/^abc(?<=c)/, "abc"), [0, "abc"]);
shouldBe(matchOf(/(?<=\u00e9)x/i, "\u00c9x"), [1, "x"]);
shouldBe(matchOf(/(?<=[\u00e0-\u00ff]{2})x/i, "\u00c9\u00c0x"), [2, "x"]);
shouldBe(matchOf(/(?<=k)x/i, "Kx"), [1, "x"]);
shouldBe(matchOf(/(?<=k)x/i, "\u212ax"), null);
shouldBe(matchOf(/(?<=a.b)x/, "a\u0100bx"), [3, "x"]);
shouldBe(matchOf(/(?<=a[^b]b)x/, "a\u0100bx"), [3, "x"]);
shouldBe(matchOf(new RegExp("(?<=\u0100{2})x"), "q\u0100\u0100x"), [3, "x"]);
shouldBe(matchOf(/(?<=\s{2})x/, "a  x"), [3, "x"]);
shouldBe(matchOf(/(?<=^\s*)x/m, "a\n  x"), [4, "x"]);
shouldBe(matchOf(/(?<=(?:ab){0}c)x/, "cx"), [1, "x"]);
shouldBe(matchOf(/(?<=a{0}c)x/, "cx"), [1, "x"]);
shouldBe(matchOf(/(?<=b(?:){5}c)x/, "bcx"), [2, "x"]);
shouldBe(matchOf(/(?<=(?=)c)x/, "cx"), [1, "x"]);
