function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function target(array)
{
    return [...array, 4, ...array];
}

function test()
{
    return target([1, 2, 3]);
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var result = test();
    shouldBe(result.length, 7);
    shouldBe(result[0], 1);
    shouldBe(result[1], 2);
    shouldBe(result[2], 3);
    shouldBe(result[3], 4);
    shouldBe(result[4], 1);
    shouldBe(result[5], 2);
    shouldBe(result[6], 3);
}
