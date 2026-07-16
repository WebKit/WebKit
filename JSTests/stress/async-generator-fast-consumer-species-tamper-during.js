// for-await fast driver must still perform the observable PromiseResolve (Promise.prototype.constructor lookup)
// when the Promise species is tampered mid-loop, after op_async_iterator_open. Async-generator variant.
// Count verified against V8 and SpiderMonkey.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

const savedCtorDesc = Object.getOwnPropertyDescriptor(Promise.prototype, "constructor");
function armReadCounter(counter) {
    Object.defineProperty(Promise.prototype, "constructor", {
        configurable: true,
        get() { counter.n++; return Promise; },
    });
}
function restoreCtor() {
    Object.defineProperty(Promise.prototype, "constructor", savedCtorDesc);
}

async function asyncGeneratorTamperDuring(tamper) {
    const counter = { n: 0 };
    let armed = false;
    async function* g() { yield 1; yield 2; yield 3; }
    const seq = [];
    for await (const x of g()) {
        seq.push(x);
        if (tamper && !armed) {
            armed = true;
            armReadCounter(counter);
        }
    }
    if (tamper)
        restoreCtor();
    return counter.n + "|" + seq.join(",");
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++)
        assert((await asyncGeneratorTamperDuring(false)) === "0|1,2,3", "warm iteration " + i);

    assert((await asyncGeneratorTamperDuring(true)) === "3|1,2,3", "async-gen tamper-during");
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

restoreCtor();
if (error)
    throw error;
assert(done, "async main() did not complete");
