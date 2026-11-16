function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(set, key)
{
    return set.has(key);
}
noInline(test);

var set = new Set();
for (var i = 0; i < 1e4; ++i) {
    set.clear();
    shouldBe(test(set, {}), false);
    shouldBe(test(set, {t:42}), false);
}
