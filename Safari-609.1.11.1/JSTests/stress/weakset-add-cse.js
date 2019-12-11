function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var key1 = {};
    var set = new WeakSet();
    var r1 = set.has(key1);
    set.add(key1);
    var r2 = set.has(key1);
    return [r1, r2];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    let [r1, r2] = test();
    shouldBe(r1, false);
    shouldBe(r2, true);
}
