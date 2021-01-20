function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var map = new Map;
for (var i = 0; i < 100; ++i)
    map.set(i, i);

function test(map) {
    var result = 0;
    for (let [a, b] of map) {
        result += a;
    }
    return result;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(test(map), 4950);
