// for-await fast driver must still perform the observable PromiseResolve (Promise.prototype.constructor lookup)
// when the Promise species is tampered mid-loop, after op_async_iterator_open. Async-from-sync variant (for
// await over a sync iterable). Count verified against V8 and SpiderMonkey.

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

async function asyncFromSyncTamperDuring(tamper) {
    const counter = { n: 0 };
    let armed = false;
    const seq = [];
    for await (const x of [1, 2, 3]) {
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
        assert((await asyncFromSyncTamperDuring(false)) === "0|1,2,3", "warm iteration " + i);

    assert((await asyncFromSyncTamperDuring(true)) === "3|1,2,3", "async-from-sync tamper-during");
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

restoreCtor();
if (error)
    throw error;
assert(done, "async main() did not complete");
