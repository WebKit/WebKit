function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function args()
{
    return arguments[1];
}
noInline(args);

shouldBe(args(), undefined);
shouldBe(args(0), undefined);
shouldBe(args(0, 1), 1);
for (var i = 0; i < 1e6; ++i)
    shouldBe(args(), undefined);
Object.prototype[1] = 42;
shouldBe(args(), 42);
