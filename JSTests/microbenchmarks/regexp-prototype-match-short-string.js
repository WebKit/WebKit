function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

const str = "abc123XYZ";

const re1 = /^([a-z]+)(\d+)([A-Z]+)$/;
const re2 = /^([a-z]+)(\d+)([A-Z]+)foo$/;

for (let i = 0; i < 1e5; i++) {
    shouldBe(re1[Symbol.match](str)[0], "abc123XYZ");
    shouldBe(re2[Symbol.match](str), null);
    shouldBe(str.match(re1)[0], "abc123XYZ");
    shouldBe(str.match(re2), null);
}
