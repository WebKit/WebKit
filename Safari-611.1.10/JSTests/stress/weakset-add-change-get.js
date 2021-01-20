function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var key1 = {};
    var set = new WeakSet();
    var res1 = set.has(key1);
    set.add(key1);
    var res2 = set.has(key1);
    set.delete(key1);
    var res3 = set.has(key1);
    return [res1, res2, res3];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var [res1, res2, res3] = test();
    shouldBe(res1, false);
    shouldBe(res2, true);
    shouldBe(res3, false);
}
