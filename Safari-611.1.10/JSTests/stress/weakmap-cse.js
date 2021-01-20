function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new WeakMap();
    var key = {};
    var key2 = {};

    map.set(key, 42);
    map.set(key2, 2017);
    if (map.has(key))
        return map.get(key);
    return 0;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), 42);
