function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let regexp = /aaaa|(bbb|cccc)/; // => [abc][abc][abc]
    shouldBe(regexp.test("bbb"), true);
    shouldBe(regexp.test("aabb"), false);
}

{
    let regexp = /aaaa|(bbb|cccc)?/; // =>
    shouldBe(regexp.test("bbb"), true);
    shouldBe(regexp.test("aabb"), true);
    shouldBe(regexp.test(""), true);
}

{
    let regexp = /aaaa|a(bbb|cccc)?/; // => [a]
    shouldBe(regexp.test("aaaa"), true);
    shouldBe(regexp.test("a"), true);
    shouldBe(regexp.test("abbb"), true);
    shouldBe(regexp.test("acccc"), true);
    shouldBe(regexp.test("dcccc"), false);
}

{
    let regexp = /aaaa|(bbb|cccc)?dd/; // => [abcd]
    shouldBe(regexp.test("bbb"), false);
    shouldBe(regexp.test("aaaa"), true);
    shouldBe(regexp.test("aabb"), false);
    shouldBe(regexp.test("dd"), true);
}

{
    let regexp = /aaaa|(bbb|cccc?)?dd/; // => [abc][abc][abc]
    shouldBe(regexp.test("bbb"), false);
    shouldBe(regexp.test("aaaa"), true);
    shouldBe(regexp.test("aabb"), false);
    shouldBe(regexp.test("dd"), true);
}

{
    let regexp = /aaaa|(b|cccc)dd/; // => [abc][acd]
    shouldBe(regexp.test("bbb"), false);
    shouldBe(regexp.test("aaaa"), true);
    shouldBe(regexp.test("aabb"), false);
    shouldBe(regexp.test("ccccdd"), true);
    shouldBe(regexp.test("ccccdd"), true);
}

{
    let regexp = /aaaaaaa|(bb?|cc?)dddddd/; // => [abc][acd]
    shouldBe(regexp.test("aaaaaaa"), true);
    shouldBe(regexp.test("bdddddd"), true);
    shouldBe(regexp.test("cdddddd"), true);
    shouldBe(regexp.test("bbdddddd"), true);
    shouldBe(regexp.test("ccdddddd"), true);
    shouldBe(regexp.test("dddddd"), false);
    shouldBe(regexp.test("dddddd"), false);
    shouldBe(regexp.test("bddddd"), false);
    shouldBe(regexp.test("cddddd"), false);
    shouldBe(regexp.test("bbddddd"), false);
    shouldBe(regexp.test("ccddddd"), false);
    shouldBe(regexp.test("aaaaaabdddddd"), true);
    shouldBe(regexp.test("aaaaaacdddddd"), true);
    shouldBe(regexp.test("aaaaaabbdddddd"), true);
    shouldBe(regexp.test("aaaaaaccdddddd"), true);
}

{
    let regexp = /\baaaaaaa|(bb?|cc?)dddddd/; // => [abc][acd]
    shouldBe(regexp.test("aaaaaaa"), true);
    shouldBe(regexp.test("bdddddd"), true);
    shouldBe(regexp.test("cdddddd"), true);
    shouldBe(regexp.test("bbdddddd"), true);
    shouldBe(regexp.test("ccdddddd"), true);
    shouldBe(regexp.test("dddddd"), false);
    shouldBe(regexp.test("dddddd"), false);
    shouldBe(regexp.test("bddddd"), false);
    shouldBe(regexp.test("cddddd"), false);
    shouldBe(regexp.test("bbddddd"), false);
    shouldBe(regexp.test("ccddddd"), false);
    shouldBe(regexp.test("baaaaaaa"), false);
    shouldBe(regexp.test("aaaaaabdddddd"), true);
    shouldBe(regexp.test("aaaaaacdddddd"), true);
    shouldBe(regexp.test("aaaaaabbdddddd"), true);
    shouldBe(regexp.test("aaaaaaccdddddd"), true);
}
