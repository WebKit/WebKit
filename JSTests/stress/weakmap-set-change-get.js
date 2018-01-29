function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var key1 = {};
    var map = new WeakMap();
    var res1 = map.get(key1);
    map.set(key1, 20);
    var res2 = map.get(key1);
    map.set(key1, 400);
    var res3 = map.get(key1);
    map.delete(key1);
    var res4 = map.get(key1);
    return [res1, res2, res3, res4];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var [res1, res2, res3, res4] = test();
    shouldBe(res1, undefined);
    shouldBe(res2, 20);
    shouldBe(res3, 400);
    shouldBe(res4, undefined);
}
