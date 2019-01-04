function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test(value)
{
    return Object.prototype.toString.call(value);
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    if (i & 0x1)
        shouldBe(test(null), `[object Null]`);
    else
        shouldBe(test(undefined), `[object Undefined]`);
}
