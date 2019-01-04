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
    switch (i % 3) {
    case 0:
        shouldBe(test(null), `[object Null]`);
        break;
    case 1:
        shouldBe(test(undefined), `[object Undefined]`);
        break;
    case 2:
        shouldBe(test(true), `[object Boolean]`);
        break;
    }
}
