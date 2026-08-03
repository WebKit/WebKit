//@ runDefault("--useLOLJIT=1")
// Regression test: op_async_iterator_open and op_async_iterator_next must be registered in the LOL
// JIT's opcode dispatch. LOLJIT gates on the DFG capability level (which accepts both ops), so a
// for-await loop compiled by LOLJIT would otherwise hit its `default: RELEASE_ASSERT_NOT_REACHED()`.
// Exercises both the fast-consumer path (genuine async generator -> op_async_iterator_open writes the
// sentinel, op_async_iterator_next enqueues) and the generic path (custom async iterable -> real
// next.call(iterator)) under --useLOLJIT=1.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

async function* genConsumer() {
    yield 10;
    yield 20;
    yield 30;
}

function customIterable() {
    let i = 0;
    return {
        [Symbol.asyncIterator]() {
            return {
                next() {
                    return Promise.resolve(i < 3 ? { value: (++i) * 100, done: false } : { value: undefined, done: true });
                }
            };
        }
    };
}

let done = false;
let error = null;

async function main() {
    let total = 0;
    for (let k = 0; k < testLoopCount; k++) {
        for await (const x of genConsumer())
            total += x;
        for await (const x of customIterable())
            total += x;
    }
    // Each outer iteration: genConsumer sums 60, customIterable sums 600 -> 660.
    assert(total === testLoopCount * 660, "total was " + total);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
