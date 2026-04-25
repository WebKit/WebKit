function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var regexp = /aaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbb/i;

for (var i = 0; i < 1e2; ++i) {
    shouldBe(regexp.test("abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccdddaaaabbbbccccddddfffff"), false);
    shouldBe(regexp.test("abcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaabbabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccdddaaaabbbbccccddddaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbfffff"), true);
    shouldBe(RegExp.leftContext, "abcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaaaabbbbaabbabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccdddaaaabbbbccccdddd");
    shouldBe(RegExp.rightContext, "fffff");
    shouldBe(regexp.test("abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccddfffffffffffffffffffffff"), false);
}
