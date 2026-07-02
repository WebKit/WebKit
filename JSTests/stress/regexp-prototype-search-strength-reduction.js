function shouldBe(a, b) {
    if (a !== b) throw new Error(`Expected ${b} but got ${a}`);
}

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(/world/[Symbol.search]("hello world"), 6);
    shouldBe(/hello/[Symbol.search]("hello world"), 0);
    shouldBe(/^hello/[Symbol.search]("hello world"), 0);
    shouldBe(/world$/[Symbol.search]("hello world"), 6);

    shouldBe(/xyz/[Symbol.search]("hello world"), -1);
    shouldBe(/^world/[Symbol.search]("hello world"), -1);
    shouldBe(/hello$/[Symbol.search]("hello world"), -1);

    shouldBe(/l/g[Symbol.search]("hello world"), 2);
    shouldBe(/o/g[Symbol.search]("hello world"), 4);

    shouldBe(/hello/y[Symbol.search]("hello world"), 0);
    shouldBe(/world/y[Symbol.search]("hello world"), -1);
}
