function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    var set = new Set();
    set.add(42);
    var res1 = set.has(42);
    map.set(42, 42);
    var res2 = set.has(42);
    return res1 + res2;
}

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), 2);
