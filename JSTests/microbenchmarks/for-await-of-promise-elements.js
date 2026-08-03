// Microbenchmark: for-await-of over a sync iterable of promises. Each element is Await-ed through the
// async-from-sync wrapper (the AsyncFromSyncIteratorContinuation Await of a genuine promise value).

var data = [];
for (var i = 0; i < 64; ++i)
    data.push(Promise.resolve(i));

async function run() {
    var sum = 0;
    for (var n = 0; n < 16e3; ++n)
        for await (var v of data)
            sum += v;
    return sum;
}

run();
drainMicrotasks();
