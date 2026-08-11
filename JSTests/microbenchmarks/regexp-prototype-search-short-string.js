function shouldBe(actual, expected) {
  if (actual !== expected) {
    throw new Error(`Expected ${expected} but got ${actual}`);
  }
}

const str = "abc123XYZ";

const re1 = /^([a-z]+)(\d+)([A-Z]+)$/;
const re2 = /^([a-z]+)(\d+)([A-Z]+)foo$/;

for (let i = 0; i < 1e6; i++) {
  shouldBe(re1[Symbol.search](str), 0);
  shouldBe(re2[Symbol.search](str), -1);
  shouldBe(str.search(re1), 0);
  shouldBe(str.search(re2), -1);
}
