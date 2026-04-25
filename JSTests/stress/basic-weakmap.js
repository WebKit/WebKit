function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new WeakMap();
    var key1 = {};
    var key2 = {};
    var key3 = [];

    var res1 = map.get(key1);
    map.set(key1, 42);
    var res2 = map.get(key1);

    shouldBe(res1, undefined);
    shouldBe(res2, 42);

    var res3 = map.get(key2);
    map.set(key3, 43);
    var res4 = map.get(key2);

    shouldBe(res3, undefined);
    shouldBe(res4, undefined);

    shouldBe(map.get(key3), 43);

    map.delete(key3);
    shouldBe(map.get(key3), undefined);

    shouldBe(map.get(key1), 42);
    map.delete(key1);
    shouldBe(map.get(key1), undefined);
    shouldBe(map.has(key1), false);

    map.set(key1, 44);
    shouldBe(map.get(key1), 44);
    shouldBe(map.has(key1), true);
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
