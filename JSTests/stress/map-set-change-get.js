function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    map.set(42, 20);
    var res1 = map.get(42);
    map.set(42, 400);
    var res2 = map.get(42);
    return [res1, res2];
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var [res1, res2] = test();
    shouldBe(res1, 20);
    shouldBe(res2, 400);
}
