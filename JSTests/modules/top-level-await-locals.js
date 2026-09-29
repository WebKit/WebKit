function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected ${String(expected)}`);
}

let a = 1, b = "b", c = { v: 3 };
let sum = 0;
for (let i = 0; i < 5; ++i) {
    let local = i * 2;
    sum += await local;
    a += await i;
    b += await String(i);
}
c.v += await 10;
shouldBe(a, 11);
shouldBe(b, "b01234");
shouldBe(c.v, 13);
shouldBe(sum, 20);
for (let i = 0; i < testLoopCount; ++i) {
    let local = i * 2;
    sum += await local;
    shouldBe(sum, 20 + i * (i + 1));
}
shouldBe(a, 11);
shouldBe(b, "b01234");
shouldBe(c.v, 13);
