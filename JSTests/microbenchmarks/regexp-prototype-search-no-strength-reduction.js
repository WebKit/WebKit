function shouldBe(a, b) {
    if (a !== b) throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < 1e6; i++) {
    shouldBe(new RegExp("wor" + "ld")[Symbol.search]("hello world"), 6);
    shouldBe(/hello/[Symbol.search]("hello" + "world" + Math.random()), 0);
}
