function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var set = new WeakSet();
    var key1 = {};
    var key2 = {};
    var key3 = [];

    var res1 = set.has(key1);
    set.add(key1);
    var res2 = set.has(key1);

    shouldBe(res1, false);
    shouldBe(res2, true);

    var res3 = set.has(key2);
    set.add(key3);
    var res4 = set.has(key2);

    shouldBe(res3, false);
    shouldBe(res4, false);

    shouldBe(set.has(key3), true);

    set.delete(key3);
    shouldBe(set.has(key3), false);

    shouldBe(set.has(key1), true);
    set.delete(key1);
    shouldBe(set.has(key1), false);

    set.add(key1);
    shouldBe(set.has(key1), true);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
