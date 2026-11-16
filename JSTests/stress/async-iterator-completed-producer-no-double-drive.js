// Regression test for the completed-producer fast path in enqueueAsyncGeneratorFastConsumer
// (runtime/JSMicrotask.cpp). When a `for await` consumes a genuine async generator that is ALREADY in
// the Completed state, op_async_iterator_next's fast branch settles { value: undefined, done: true }
// immediately AND schedules an AsyncGeneratorFastConsumerResume microtask for the consumer frame. That
// completed branch must still set the consumer's SuppressFastResume flag; otherwise the consumer is
// driven twice -- once by that microtask, once by the normal await-Promise machinery its own driver
// (driveAsyncFunction / asyncFunctionGeneratorBodyCall / asyncGeneratorBodyCall) attaches when it
// suspends at the following op_yield -- re-entering an already-resolving frame and double-advancing it.
//
// We exercise every driver: an async-function consumer's initial run (driveAsyncFunction) and its
// subsequent resumes (asyncFunctionGeneratorBodyCall), plus an async-generator consumer
// (asyncGeneratorBodyCall), each re-iterating an exhausted genuine async generator. Warmed hot enough
// to reach the upper JIT tiers. With the bug, the stray extra resume inflates counters / sums, throws,
// or hangs (main never completes); with the fix every count is exact.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function makeGen() {
    return (async function* () {
        yield 1;
        yield 2;
    })();
}

// Counts how many times each consumer's post-loop tail actually executes. A double-drive would run a
// tail more than once (or corrupt the frame), so these must exactly match the number of calls.
let tailRuns = 0;

// Async-function consumer. First loop exhausts g; second loop re-iterates the now-Completed g (body
// must never run); then a real await forces a second suspension so any stray resume is observable.
async function reconsume() {
    const g = makeGen();
    let sum = 0;
    for await (const x of g)
        sum += x;               // 1 + 2
    for await (const x of g)
        sum += 1000;            // g Completed -> must NOT run
    await Promise.resolve();    // suspend again; a stray resume would double-advance past here
    tailRuns++;
    return sum;
}

// Async-function consumer whose very first suspension is the completed-producer next (initial-run path,
// driven by driveAsyncFunction rather than asyncFunctionGeneratorBodyCall).
async function consumeAlreadyExhausted(g) {
    let count = 0;
    for await (const x of g)
        count += 1;             // g Completed -> must NOT run
    tailRuns++;
    return count;
}

// Async-generator consumer over a completed producer (asyncGeneratorBodyCall driver).
async function* genConsumeExhausted(g) {
    for await (const x of g)
        yield x;                // g Completed -> must NOT yield
    yield -1;                   // sentinel, must be produced exactly once
    tailRuns++;
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    let expectedTailRuns = 0;

    for (let k = 0; k < N; k++) {
        const r = await reconsume();
        assert(r === 3, "reconsume expected 3, got " + r);
        expectedTailRuns++;

        const g = makeGen();
        for await (const _ of g) { }               // exhaust
        const c = await consumeAlreadyExhausted(g);
        assert(c === 0, "consumeAlreadyExhausted expected 0, got " + c);
        expectedTailRuns++;

        const g2 = makeGen();
        for await (const _ of g2) { }              // exhaust
        const collected = [];
        for await (const y of genConsumeExhausted(g2))
            collected.push(y);
        assert(collected.length === 1 && collected[0] === -1,
            "genConsumeExhausted produced " + JSON.stringify(collected));
        expectedTailRuns++;
    }

    assert(tailRuns === expectedTailRuns, "tailRuns expected " + expectedTailRuns + ", got " + tailRuns);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
