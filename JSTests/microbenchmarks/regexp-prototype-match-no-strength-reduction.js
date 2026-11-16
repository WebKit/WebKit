function shouldBe(a, b) {
    if (JSON.stringify(a) !== JSON.stringify(b))
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

for (let i = 0; i < 1e5; i++) {
    shouldBe(new RegExp("wor" + "ld")[Symbol.match]("hello world")[0], "world");
    shouldBe(/hello/[Symbol.match]("hello" + "world" + Math.random())[0], "hello");
}
