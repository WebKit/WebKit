// Microbenchmark: fast iteration over map.values() (no key load needed).

function mapValuesSum(map) {
    var sum = 0;
    for (var v of map.values())
        sum += v;
    return sum;
}
noInline(mapValuesSum);

var map = new Map();
for (var i = 0; i < 1024; ++i)
    map.set(i, i * 2);

var iters = 2e4;
for (var i = 0; i < iters; ++i)
    mapValuesSum(map);
