function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function inc(a) {
    return ++a;
}
noInline(inc);

{
    for (var i = 0; i < 100000; ++i)
        shouldBe(inc(BigInt(i)), BigInt(i + 1));
}
{
    let max = 0x7fffffffn;
    let result = 0x80000000n;
    for (var i = 0; i < 100000; ++i)
        shouldBe(inc(max), result);
}
