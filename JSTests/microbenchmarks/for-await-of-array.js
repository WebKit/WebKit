// Microbenchmark: for-await-of over a sync array iterable (async-from-sync wrapper; fast Array modes).

var data = [];
for (var i = 0; i < 128; ++i)
    data.push(i);

async function values() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var v of data)
            sum += v;
    return sum;
}

async function keys() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var k of data.keys())
            sum += k;
    return sum;
}

async function entries() {
    var sum = 0;
    for (var n = 0; n < 6e3; ++n)
        for await (var [k, v] of data.entries())
            sum += k + v;
    return sum;
}

values();
keys();
entries();
drainMicrotasks();
