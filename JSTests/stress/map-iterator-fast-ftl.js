// FTL fast path for map.keys()/values()/entries() iteration.
//@ runFTLNoCJIT

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + ' (expected ' + expected + ')');
}

function values(map) {
    var sum = 0;
    for (var v of map.values()) sum += v;
    return sum;
}

function keys(map) {
    var sum = 0;
    for (var k of map.keys()) sum += k;
    return sum;
}

function entries(map) {
    var sum = 0;
    for (var [k, v] of map.entries()) sum += k * 1000 + v;
    return sum;
}

var map = new Map();
for (var i = 0; i < 10; ++i)
    map.set(i, i + 1);

var expectedEntries = 0;
for (var i = 0; i < 10; ++i) expectedEntries += i * 1000 + (i + 1);

for (var i = 0; i < testLoopCount; ++i) {
    shouldBe(values(map), 55);
    shouldBe(keys(map), 45);
    shouldBe(entries(map), expectedEntries);
}
