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
    set.add(key2);
    if (set.has(key))
        return set.has(key);
    return 0;
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    shouldBe(test(), true);
