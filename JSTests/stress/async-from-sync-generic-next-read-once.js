// for-await over a sync iterable with no @@asyncIterator wraps it in %AsyncFromSyncIteratorPrototype%. Per
// GetIteratorFromMethod (https://tc39.es/ecma262/#sec-getiteratorfrommethod), the sync iterator's `next` method
// is read exactly once when the wrapper is built and then reused for every drive -- it must never be re-read
// per iteration. Here the sync iterator is a plain object (Generic iteration mode), and `next` is an accessor
// so we can count reads: nextReads === 1 regardless of element count.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function makeIterable(limit) {
    let nextReads = 0;
    let nextCalls = 0;
    const iterable = {
        [Symbol.iterator]() {
            let i = 0;
            return {
                get next() {
                    nextReads++;
                    // A fresh function each read: if `next` were read more than once, the first-captured
                    // method would be replaced, so a wrong read count also perturbs behavior.
                    return function () {
                        nextCalls++;
                        if (i < limit)
                            return { value: i++, done: false };
                        return { value: undefined, done: true };
                    };
                }
            };
        }
    };
    return { iterable, reads: () => nextReads, calls: () => nextCalls };
}

async function run(limit) {
    const record = makeIterable(limit);
    const seq = [];
    for await (const x of record.iterable)
        seq.push(x);

    assert(record.reads() === 1, `next read ${record.reads()} times, expected 1 (limit=${limit})`);
    // The captured method is invoked once per element plus once for the terminating { done: true }.
    assert(record.calls() === limit + 1, `next called ${record.calls()} times, expected ${limit + 1}`);

    let expected = "";
    for (let i = 0; i < limit; i++)
        expected += (i ? "," : "") + i;
    assert(seq.join(",") === expected, `bad sequence: ${seq.join(",")}, expected: ${expected}`);
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++)
        await run(3);
    // Also exercise an empty and a longer iterable.
    await run(0);
    await run(7);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
