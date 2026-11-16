function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error(`Expected ${JSON.stringify(expected)} but got ${JSON.stringify(actual)}`);
}

const str = ("X".repeat(50) + "the_end").repeat(2);

const re1 = /the_end$/;
const re2 = /^X{50}the_end$/;
const re3 = /the_end/g;

for (let i = 0; i < 1e5; i++) {
    shouldBe(re1[Symbol.match](str)[0], "the_end");
    shouldBe(re2[Symbol.match](str), null);
    shouldBe(re3[Symbol.match](str), ["the_end", "the_end"]);
    shouldBe(str.match(re1)[0], "the_end");
    shouldBe(str.match(re2), null);
    shouldBe(str.match(re3), ["the_end", "the_end"]);
}
