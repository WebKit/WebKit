function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function sub(a, b) {
    return a - b;
}
noInline(sub);

{
    for (var i = 0; i < 100000; ++i)
        shouldBe(sub(BigInt(i), 2n), BigInt(i - 2));
}
{
    let min = -0x7fffffffn - 1n;
    let result = -0x80000002n;
    for (var i = 0; i < 100000; ++i)
        shouldBe(sub(min, 2n), result);
}
