function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test()
{
    var map = new Map();
    var r1 = map.get(42);
    map.set(42, 42);
    var r2 = map.get(42);
    return [r1, r2];
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    let [r1, r2] = test();
    shouldBe(r1, undefined);
    shouldBe(r2, 42);
}
