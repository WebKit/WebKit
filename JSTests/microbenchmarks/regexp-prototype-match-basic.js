function shouldBe(a, b) {
    if (JSON.stringify(a) !== JSON.stringify(b))
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

const str = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzfoo";
const re1 = /foo/;
const re2 = /bar/;
const re3 = /[a-z]/g;
for (let i = 0; i < 1e4; i++) {
    shouldBe(re1[Symbol.match](str)[0], "foo");
    shouldBe(re2[Symbol.match](str), null);
    shouldBe(re3[Symbol.match](str).length, 81);
    shouldBe(str.match(re1)[0], "foo");
    shouldBe(str.match(re2), null);
    shouldBe(str.match(re3).length, 81);
}
