function shouldBe(actual, expected, message) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(message + ": expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

(function testDecreasingMinimumSize() {
    var re = /dddd|ccc|bb|a/;
    shouldBe(re.exec("a"), ["a"], "dddd|ccc|bb|a on 'a'");
    shouldBe(re.exec("bb"), ["bb"], "dddd|ccc|bb|a on 'bb'");
    shouldBe(re.exec("ccc"), ["ccc"], "dddd|ccc|bb|a on 'ccc'");
    shouldBe(re.exec("dddd"), ["dddd"], "dddd|ccc|bb|a on 'dddd'");
    shouldBe(re.exec("x"), null, "dddd|ccc|bb|a on 'x'");
    shouldBe(re.exec(""), null, "dddd|ccc|bb|a on ''");
})();

(function testIncreasingMinimumSize() {
    var re = /a|bb|ccc|dddd/;
    shouldBe(re.exec("a"), ["a"], "a|bb|ccc|dddd on 'a'");
    shouldBe(re.exec("bb"), ["bb"], "a|bb|ccc|dddd on 'bb'");
    shouldBe(re.exec("ccc"), ["ccc"], "a|bb|ccc|dddd on 'ccc'");
    shouldBe(re.exec("dddd"), ["dddd"], "a|bb|ccc|dddd on 'dddd'");
})();

(function testMixedMinimumSizes() {
    var re = /abc|d|efgh|ij/;
    shouldBe(re.exec("abc"), ["abc"], "abc|d|efgh|ij on 'abc'");
    shouldBe(re.exec("d"), ["d"], "abc|d|efgh|ij on 'd'");
    shouldBe(re.exec("efgh"), ["efgh"], "abc|d|efgh|ij on 'efgh'");
    shouldBe(re.exec("ij"), ["ij"], "abc|d|efgh|ij on 'ij'");
    shouldBe(re.exec("x"), null, "abc|d|efgh|ij on 'x'");
})();

(function testBoundaryInputLength() {
    var re = /abcdef|abc|a/;
    shouldBe(re.exec("abc"), ["abc"], "abcdef|abc|a on 'abc'");
    shouldBe(re.exec("ab"), ["a"], "abcdef|abc|a on 'ab'");
    shouldBe(re.exec("a"), ["a"], "abcdef|abc|a on 'a'");
    shouldBe(re.exec(""), null, "abcdef|abc|a on ''");
})();

(function testRepeatingAlternatives() {
    var re = /xxxx|xx|x/;
    shouldBe(re.exec("---x---"), ["x"], "xxxx|xx|x on '---x---'");
    shouldBe(re.exec("---xx---"), ["xx"], "xxxx|xx|x on '---xx---'");
    shouldBe(re.exec("---xxxx---"), ["xxxx"], "xxxx|xx|x on '---xxxx---'");
    shouldBe(re.exec("---xxx---"), ["xx"], "xxxx|xx|x on '---xxx---' matches 'xx' (first matching alt)");

    var result = re.exec("---xx---");
    shouldBe(result.index, 3, "xxxx|xx|x match position in '---xx---'");
})();

(function testWithCaptures() {
    var re = /(abcd)|(ab)|(a)/;
    var result = re.exec("ab");
    shouldBe(result[0], "ab", "capture: full match");
    shouldBe(result[1], undefined, "capture: group 1 should not match");
    shouldBe(result[2], "ab", "capture: group 2 should match");
    shouldBe(result[3], undefined, "capture: group 3 should not match");

    result = re.exec("a");
    shouldBe(result[0], "a", "capture: full match for 'a'");
    shouldBe(result[1], undefined, "capture: group 1 should not match for 'a'");
    shouldBe(result[2], undefined, "capture: group 2 should not match for 'a'");
    shouldBe(result[3], "a", "capture: group 3 should match for 'a'");
})();

(function testManyAlternatives() {
    var re = /abcdefgh|abcdefg|abcdef|abcde|abcd|abc|ab|a/;
    shouldBe(re.exec("a"), ["a"], "8 alts on 'a'");
    shouldBe(re.exec("ab"), ["ab"], "8 alts on 'ab'");
    shouldBe(re.exec("abc"), ["abc"], "8 alts on 'abc'");
    shouldBe(re.exec("abcd"), ["abcd"], "8 alts on 'abcd'");
    shouldBe(re.exec("abcde"), ["abcde"], "8 alts on 'abcde'");
    shouldBe(re.exec("abcdef"), ["abcdef"], "8 alts on 'abcdef'");
    shouldBe(re.exec("abcdefg"), ["abcdefg"], "8 alts on 'abcdefg'");
    shouldBe(re.exec("abcdefgh"), ["abcdefgh"], "8 alts on 'abcdefgh'");
})();

(function testZeroMinimumSize() {
    var re = /abc|/;
    shouldBe(re.exec("abc"), ["abc"], "abc| on 'abc'");
    shouldBe(re.exec("x"), [""], "abc| on 'x' (empty match)");
    shouldBe(re.exec(""), [""], "abc| on '' (empty match)");
})();

(function testJITTierUp() {
    var re = /xxxxx|xxx|x/;
    for (var i = 0; i < 1000; i++) {
        shouldBe(re.exec("x"), ["x"], "JIT tier-up iteration " + i);
        shouldBe(re.exec("xxx"), ["xxx"], "JIT tier-up iteration " + i);
        shouldBe(re.exec("xxxxx"), ["xxxxx"], "JIT tier-up iteration " + i);
        shouldBe(re.exec("xx"), ["x"], "JIT tier-up iteration " + i);
        shouldBe(re.exec("xxxx"), ["xxx"], "JIT tier-up iteration " + i);
    }
})();

(function testUnicode() {
    var re = /\u{1F600}\u{1F601}|\u{1F600}|a/u;
    shouldBe(re.exec("a"), ["a"], "unicode on 'a'");
    shouldBe(re.exec("\u{1F600}"), ["\u{1F600}"], "unicode on single emoji");
    shouldBe(re.exec("\u{1F600}\u{1F601}"), ["\u{1F600}\u{1F601}"], "unicode on two emojis");
})();

(function testGlobalMatching() {
    var re = /aaa|aa|a/g;
    var str = "aaaaaa";
    var matches = str.match(re);
    shouldBe(matches, ["aaa", "aaa"], "global matching 'aaaaaa'");

    re = /aaa|aa|a/g;
    str = "aaaaa";
    matches = str.match(re);
    shouldBe(matches, ["aaa", "aa"], "global matching 'aaaaa'");
})();

(function testStickyFlag() {
    var re = /xxxx|xx|x/y;
    re.lastIndex = 2;
    shouldBe(re.exec("--xx--"), ["xx"], "sticky at index 2");
    shouldBe(re.lastIndex, 4, "sticky lastIndex after match");
    shouldBe(re.exec("--xx--"), null, "sticky at index 4 (no match)");
})();

(function testCaseInsensitive() {
    var re = /ABCD|AB|A/i;
    shouldBe(re.exec("ab"), ["ab"], "case insensitive on 'ab'");
    shouldBe(re.exec("a"), ["a"], "case insensitive on 'a'");
    shouldBe(re.exec("abcd"), ["abcd"], "case insensitive on 'abcd'");
})();
