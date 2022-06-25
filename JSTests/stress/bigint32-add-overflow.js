function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function add(a, b) {
    return a + b;
}
noInline(add);

{
    for (var i = 0; i < 100000; ++i)
        shouldBe(add(BigInt(i), 2n), BigInt(i + 2));
}
{
    let max = 0x7fffffffn;
    let result = 0x80000001n;
    for (var i = 0; i < 100000; ++i)
        shouldBe(add(max, 2n), result);
}
