function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var regexp = /aaaabbbbccccddddaaaabbbbccccdddaaaabbbbccccdddd/i;

for (var i = 0; i < 1e2; ++i) {
    shouldBe(regexp.test("abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccdddaaaabbbbccccddddfffff"), true);
    shouldBe(RegExp.leftContext, "abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefg");
    shouldBe(RegExp.rightContext, "fffff");
    shouldBe(regexp.test("abcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgabcdefgaaaabbbbccccddddaaaabbbbccccddfffffffffffffffffffffff"), false);
}
