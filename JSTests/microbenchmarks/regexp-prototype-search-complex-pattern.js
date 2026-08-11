function shouldBe(actual, expected) {
  if (actual !== expected) {
    throw new Error(`Expected ${expected} but got ${actual}`);
  }
}

const str = "abc123XYZ_foo_bar_baz_987654321".repeat(2);

const re1 = /abc\d+XYZ_foo_bar_baz_\d+/;
const re2 = /abc\d+(?=XXX)(?:_foo_)(?:bar_)(?:baz_)\d+/;

for (let i = 0; i < 1e6; i++) {
  shouldBe(re1[Symbol.search](str), 0);
  shouldBe(re2[Symbol.search](str), -1);
  shouldBe(str.search(re1), 0);
  shouldBe(str.search(re2), -1);
}
