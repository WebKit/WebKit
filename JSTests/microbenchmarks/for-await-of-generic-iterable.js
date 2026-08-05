// Microbenchmark: for-await-of over a user-defined sync iterable (async-from-sync wrapper; Generic cooperative
// driver -- next() is a real JS call, driven directly without allocating a per-step promise).

function makeIterable(length) {
    return {
        [Symbol.iterator]() {
            var i = 0;
            return {
                next() {
                    return i < length ? { value: i++, done: false } : { value: undefined, done: true };
                }
            };
        }
    };
}
var iterable = makeIterable(64);

async function run() {
    var sum = 0;
    for (var n = 0; n < 16e3; ++n)
        for await (var v of iterable)
            sum += v;
    return sum;
}

run();
drainMicrotasks();
