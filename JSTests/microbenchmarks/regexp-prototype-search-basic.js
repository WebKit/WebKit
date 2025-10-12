function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const str = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzfoo";
const re1 = /foo/;
const re2 = /bar/;
for (let i = 0; i < 1e6; i++) {
    shouldBe(re1[Symbol.search](str), 78);
    shouldBe(re2[Symbol.search](str), -1);
    shouldBe(str.search(re1), 78);
    shouldBe(str.search(re2), -1);
}
