function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var set = new Set();
    var r1 = set.has(42);
    set.add(42);
    var r2 = set.has(42);
    return [r1, r2];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    let [r1, r2] = test();
    shouldBe(r1, false);
    shouldBe(r2, true);
}
