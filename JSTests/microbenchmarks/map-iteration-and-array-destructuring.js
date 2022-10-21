function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(map)
{
    var result = 0;
    for (let [key, value] of map) {
        result += key;
        result += value;
    }
    return result;
}
noInline(test);

var map = new Map();
for (var i = 0; i < 100; ++i)
    map.set(i, 42);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test(map), 9150);
