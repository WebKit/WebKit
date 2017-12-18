function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    return [...[1.2, 2.3, 3.4]];
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var [a, b, c] = test();
    shouldBe(a, 1.2);
    shouldBe(b, 2.3);
    shouldBe(c, 3.4);
}
