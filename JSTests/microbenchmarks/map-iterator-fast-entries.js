// Microbenchmark: fast iteration over map.entries() (allocates [key, value] per step).

function mapEntriesSum(map) {
    var sum = 0;
    for (var [k, v] of map.entries())
        sum += k + v;
    return sum;
}
noInline(mapEntriesSum);

var map = new Map();
for (var i = 0; i < 1024; ++i)
    map.set(i, i * 2);

var iters = 1e4;
for (var i = 0; i < iters; ++i)
    mapEntriesSum(map);
