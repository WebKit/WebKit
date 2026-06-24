function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

const str = "abc123XYZ_foo_bar_baz_987654321".repeat(2);

const re1 = /abc\d+XYZ_foo_bar_baz_\d+/;
const re2 = /abc\d+(?=XXX)(?:_foo_)(?:bar_)(?:baz_)\d+/;
const re3 = /\d+/g;

for (let i = 0; i < 1e4; i++) {
    shouldBe(re1[Symbol.match](str)[0], "abc123XYZ_foo_bar_baz_987654321");
    shouldBe(re2[Symbol.match](str), null);
    shouldBe(re3[Symbol.match](str), ["123", "987654321", "123", "987654321"]);
    shouldBe(str.match(re1)[0], "abc123XYZ_foo_bar_baz_987654321");
    shouldBe(str.match(re2), null);
    shouldBe(str.match(re3), ["123", "987654321", "123", "987654321"]);
}
