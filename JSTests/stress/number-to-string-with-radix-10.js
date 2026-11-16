function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test()
{
    for (var i = 0; i < 10; ++i)
        shouldBe(i.toString(10), "" + i);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
