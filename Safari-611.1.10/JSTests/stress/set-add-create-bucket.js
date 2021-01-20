function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var set = new Set();
    var res1 = set.has(42);
    set.add(42);
    var res2 = set.has(42);
    set.add(42);
    var res3 = set.has(42);
    set.delete(42);
    var res4 = set.has(42);
    return [res1, res2, res3, res4];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var [res1, res2, res3, res4] = test();
    shouldBe(res1, false);
    shouldBe(res2, true);
    shouldBe(res3, true);
    shouldBe(res4, false);
}
