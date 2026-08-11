function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let i = 2;
    let j = 3;
    shouldBe(2 ** 3, 8);
    shouldBe(i ** 3, 8);
    shouldBe(2 ** j, 8);
    shouldBe(i ** j, 8);
}

{
    shouldBe(2 ** 3 ** 3, 134217728);
    shouldBe(2 ** 3 + 3, 11);
    shouldBe(2 ** 3 + 3 ** 3, 35);
    shouldBe(2 ** 3 * 3, 24);
    shouldBe(2 ** 3 * 3 ** 3, 216);

    shouldBe(2 + 3 ** 3, 29);
    shouldBe(2 * 3 ** 3, 54);
}

{
    let i = 2;
    i **= 4;
    shouldBe(i, 16);
    i **= 1 + 1;
    shouldBe(i, 256);
}

for (let i = 0; i < 1e4; ++i) {
    let a = Math.random();
    let b = Math.random();
    shouldBe(a ** b, Math.pow(a, b));
}
