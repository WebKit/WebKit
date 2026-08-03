// Regression test for the double-drive segfault fixed in asyncGeneratorSuspend (runtime/JSMicrotask.cpp).
// An async generator that is itself a `for await` consumer of another async generator takes
// op_async_iterator_next's fast-enqueue branch, which sets the consumer's SuppressFastResume flag and
// arranges its resume via a JSSlimPromiseReaction on the producer. When such a consumer is driven
// DIRECTLY by .next() (the inline AsyncGeneratorPrototype.next path -> asyncGeneratorSuspend host fn),
// rather than by an outer for-await, asyncGeneratorSuspend must honor SuppressFastResume too; otherwise
// the consumer is resumed twice (once by the producer's reaction, once by the normal await machinery),
// corrupting the state machine and crashing. Every other driver already had this guard.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function makeProducer() {
    return (async function* () { yield 1; yield 2; yield 3; })();
}

// Async-generator consumer, driven below by manual .next() (not by a for-await).
async function* makeConsumer(producer) {
    let hits = 0;
    for await (const x of producer) {
        hits++;
        yield x * 10 + hits;   // 11, 22, 33
    }
    yield 999;                 // sentinel: emitted exactly once, after the loop
}

// Manually drive an async generator to exhaustion via awaited .next() calls.
async function driveToArray(gen) {
    const out = [];
    let r;
    while (!(r = await gen.next()).done)
        out.push(r.value);
    return out;
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount; // warm hot enough to reach the upper JIT tiers.
    for (let k = 0; k < N; k++) {
        const collected = await driveToArray(makeConsumer(makeProducer()));
        assert(collected.length === 4, "expected 4 values, got " + collected.length);
        assert(collected[0] === 11 && collected[1] === 22 && collected[2] === 33 && collected[3] === 999,
            "unexpected values: " + collected.join(","));

        // Also drive one where the consumer is closed early via .return() mid-stream.
        const g = makeConsumer(makeProducer());
        const first = await g.next();
        assert(first.value === 11 && !first.done, "first: " + JSON.stringify(first));
        const ret = await g.return("bye");
        assert(ret.done === true && ret.value === "bye", "return: " + JSON.stringify(ret));
        const after = await g.next();
        assert(after.done === true && after.value === undefined, "after-return: " + JSON.stringify(after));
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
