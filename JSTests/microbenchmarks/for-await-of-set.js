// Microbenchmark: for-await-of over a sync Set iterable (async-from-sync wrapper; fast Set modes).

var set = new Set();
for (var i = 0; i < 128; ++i)
    set.add(i);

async function values() {
    var sum = 0;
    for (var n = 0; n < 8e3; ++n)
        for await (var v of set)
            sum += v;
    return sum;
}

async function entries() {
    var sum = 0;
    for (var n = 0; n < 8e3; ++n)
        for await (var [a, b] of set.entries())
            sum += a + b;
    return sum;
}

values();
entries();
drainMicrotasks();
