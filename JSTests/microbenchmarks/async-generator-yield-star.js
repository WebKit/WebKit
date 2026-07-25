// Microbenchmark: `yield*` delegation inside an async generator.
//
// createAsyncGen delegates its whole output to `list` via `yield*`. This exercises the
// op_async_iterator_open / op_async_iterator_next path (GetIterator(async) + the delegated
// next()/await loop) rather than the old generic manual iterator-protocol loop, and it lets the
// delegation participate in the fast async-generator consumer/driver optimizations. The result is
// driven by a `for await` consumer, so both the producer's `yield*` and the consumer's iteration
// are measured end to end.

var list = [];
for (var i = 0; i < 100; ++i)
    list.push(i);

async function* createAsyncGen(list) {
    yield* list;
}

async function run() {
    var sum = 0;
    for (var n = 0; n < 15e3; ++n) {
        for await (var v of createAsyncGen(list))
            sum += v;
    }
    return sum;
}

run();
drainMicrotasks();
