function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function dec(a) {
    return --a;
}
noInline(dec);

{
    for (var i = 0; i < 100000; ++i)
        shouldBe(dec(BigInt(i)), BigInt(i - 1));
}
{
    let min = -0x7fffffffn - 1n;
    let result = -0x80000001n;
    for (var i = 0; i < 100000; ++i)
        shouldBe(dec(min), result);
}
