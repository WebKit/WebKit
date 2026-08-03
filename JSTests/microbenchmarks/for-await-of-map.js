// Microbenchmark: for-await-of over a sync Map iterable (async-from-sync wrapper; fast Map modes).

var map = new Map();
for (var i = 0; i < 128; ++i)
    map.set(i, i * 2);

async function entries() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var [k, v] of map)
            sum += k + v;
    return sum;
}

async function keys() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var k of map.keys())
            sum += k;
    return sum;
}

async function values() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var v of map.values())
            sum += v;
    return sum;
}

entries();
keys();
values();
drainMicrotasks();
