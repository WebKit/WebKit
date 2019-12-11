function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    map.set(42, 42);
    var res1 = map.get(42);
    map.set(42, 100);
    var res2 = map.get(42);
    return res1 + res2;
}

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), 142);
