function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var set = new Set();
    var res1 = set.has(42);
    set.add(42);
    var res2 = set.has(42);
    return [res1, res2];
}

for (var i = 0; i < 1e6; ++i) {
    var [res1, res2] = test();
    shouldBe(res1, false);
    shouldBe(res2, true);
}
