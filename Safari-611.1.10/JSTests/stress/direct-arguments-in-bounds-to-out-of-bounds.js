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

for (var i = 0; i < 1e6; ++i)
    shouldBe(args(0, 1, 2), 1);

for (var i = 0; i < 1e6; ++i)
    shouldBe(args(0), undefined);
