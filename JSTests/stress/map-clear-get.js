function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(map, key)
{
    return map.get(key);
}
noInline(test);

var map = new Map();
for (var i = 0; i < 1e4; ++i) {
    map.clear();
    shouldBe(test(map, {}), undefined);
    shouldBe(test(map, {t:42}), undefined);
}
