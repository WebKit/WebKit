// The sync iterator's `next` method is captured once when %AsyncFromSyncIteratorPrototype% is built and reused
// for every drive. Replacing iterator.next mid-loop must therefore have no effect: the original method keeps
// being called and iteration proceeds unchanged. Generic sync iterator (plain object).

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function makeIterable(limit) {
    let i = 0;
    let originalCalls = 0;
    let replacementCalls = 0;
    const iterator = {
        next() {
            originalCalls++;
            if (i < limit)
                return { value: i++, done: false };
            return { value: undefined, done: true };
        }
    };
    return {
        iterable: { [Symbol.iterator]() { return iterator; } },
        replaceNext() {
            iterator.next = function () {
                replacementCalls++;
                return { value: -1, done: true };
            };
        },
        originalCalls: () => originalCalls,
        replacementCalls: () => replacementCalls,
    };
}

async function run(limit) {
    const record = makeIterable(limit);
    const seq = [];
    let replaced = false;
    for await (const x of record.iterable) {
        seq.push(x);
        if (!replaced) {
            replaced = true;
            record.replaceNext(); // must be ignored: the wrapper already captured the original `next`.
        }
    }

    let expected = "";
    for (let i = 0; i < limit; i++)
        expected += (i ? "," : "") + i;
    assert(seq.join(",") === expected, `bad sequence: ${seq.join(",")}, expected: ${expected}`);
    assert(record.replacementCalls() === 0, `replacement next called ${record.replacementCalls()} times, expected 0`);
    assert(record.originalCalls() === limit + 1, `original next called ${record.originalCalls()} times, expected ${limit + 1}`);
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++)
        await run(3);
    await run(5);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
