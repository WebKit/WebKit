// Microbenchmark: fast iteration over map.keys() (no value load needed).

function mapKeysSum(map) {
    var sum = 0;
    for (var k of map.keys())
        sum += k;
    return sum;
}
noInline(mapKeysSum);

var map = new Map();
for (var i = 0; i < 1024; ++i)
    map.set(i, i * 2);

var iters = 2e4;
for (var i = 0; i < iters; ++i)
    mapKeysSum(map);
