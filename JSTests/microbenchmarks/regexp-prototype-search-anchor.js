function shouldBe(actual, expected) {
  if (actual !== expected) {
    throw new Error(`Expected ${expected} but got ${actual}`);
  }
}

const str = ("X".repeat(50) + "the_end").repeat(2);

const re1 = /the_end$/;
const re2 = /^X{50}the_end$/;

for (let i = 0; i < 1e6; i++) {
  shouldBe(re1[Symbol.search](str), 107);
  shouldBe(re2[Symbol.search](str), -1);
  shouldBe(str.search(re1), 107);
  shouldBe(str.search(re2), -1);
}
