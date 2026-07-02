function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

{
    let a0 = new Uint8Array(100);
    shouldBe(a0[3], 0);
    shouldBe(a0[4], 0);
    a0.fill(42, 3, 4);
    shouldBe(a0[3], 42);
    shouldBe(a0[4], 0);
}
{
    let a0 = new Uint8Array(4);
    shouldBe(a0[0], 0);
    a0.fill(42, 0, 0);
    shouldBe(a0[0], 0);
    a0.fill(42, 3, 0);
    for (let i = 0; i < 4; ++i)
        shouldBe(a0[i], 0);
}
