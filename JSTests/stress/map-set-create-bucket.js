function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    var res1 = map.get(42);
    map.set(42, 20);
    var res2 = map.get(42);
    map.set(42, 400);
    var res3 = map.get(42);
    return [res1, res2, res3];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var [res1, res2, res3] = test();
    shouldBe(res1, undefined);
    shouldBe(res2, 20);
    shouldBe(res3, 400);
}
