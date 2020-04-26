function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function mul(a, b) {
    return a * b;
}
noInline(mul);

{
    for (var i = 0; i < 100000; ++i)
        shouldBe(mul(BigInt(i), 2n), BigInt(i * 2));
}
{
    let max = 0x7fffffffn;
    let result = 0xfffffffen;
    for (var i = 0; i < 100000; ++i)
        shouldBe(mul(max, 2n), result);
}
