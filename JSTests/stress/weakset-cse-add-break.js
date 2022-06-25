function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var set = new WeakSet();
    var key = {};
    var key2 = {};

    set.add(key);
    var res1 = set.has(key);
    set.add(key2);
    var res2 = set.has(key);
    return [res1, res2];
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    var [res1, res2] = test();
    shouldBe(res1, true);
    shouldBe(res2, true);
}
