function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    var weakMap = new WeakMap();
    var key = {};

    var res1 = weakMap.get(key);
    map.set(key, key);
    var res2 = weakMap.get(key);
    weakMap.set(key, 42);
    var res3 = weakMap.get(key);
    return [undefined, undefined, 42];
}

for (var i = 0; i < 1e5; ++i) {
    var [res1, res2, res3] = test();
    shouldBe(res1, undefined);
    shouldBe(res2, undefined);
    shouldBe(res3, 42);
}
