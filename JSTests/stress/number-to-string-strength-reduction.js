function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var target = 42;
    return target.toString(16);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), `2a`);
