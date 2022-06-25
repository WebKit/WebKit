function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var key1 = {};
    var map = new WeakMap();
    var r1 = map.get(key1);
    map.set(key1, 42);
    var r2 = map.get(key1);
    return [r1, r2];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    let [r1, r2] = test();
    shouldBe(r1, undefined);
    shouldBe(r2, 42);
}
