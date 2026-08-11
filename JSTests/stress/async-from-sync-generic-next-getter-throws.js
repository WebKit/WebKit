// When the sync iterator's `next` accessor throws while %AsyncFromSyncIteratorPrototype% is being built
// (GetIteratorFromMethod's Get(iterator, "next")), the abrupt completion must reject the for-await, and the
// accessor must be observed exactly once (the read is not retried). Generic sync iterator (plain object).

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

const sentinel = new Error("next getter threw");

function makeThrowingIterable() {
    let nextReads = 0;
    const iterable = {
        [Symbol.iterator]() {
            return {
                get next() {
                    nextReads++;
                    throw sentinel;
                }
            };
        }
    };
    return { iterable, reads: () => nextReads };
}

async function run() {
    const record = makeThrowingIterable();
    let caught = null;
    let entered = false;
    try {
        for await (const x of record.iterable)
            entered = true;
        throw new Error("for-await should have rejected");
    } catch (e) {
        caught = e;
    }
    assert(caught === sentinel, `wrong rejection: ${String(caught)}`);
    assert(!entered, "loop body should never run");
    assert(record.reads() === 1, `next read ${record.reads()} times, expected 1`);
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++)
        await run();
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
