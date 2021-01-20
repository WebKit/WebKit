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
    var res1 = map.get(key);
    map.set(key2, 2017);
    var res2 = map.get(key);
    return [res1, res2];
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var [res1, res2] = test();
    shouldBe(res1, 42);
    shouldBe(res2, 42);
}
