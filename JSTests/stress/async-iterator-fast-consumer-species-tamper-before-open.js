// for-await fast driver must still perform the observable PromiseResolve (Promise.prototype.constructor lookup)
// when the Promise species is tampered before the loop. Counts verified against V8 and SpiderMonkey.

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

async function asyncGeneratorTamperBefore(tamper) {
    const counter = { n: 0 };
    async function* g() { yield 1; yield 2; yield 3; }
    if (tamper)
        armReadCounter(counter);
    const seq = [];
    for await (const x of g())
        seq.push(x);
    if (tamper)
        restoreCtor();
    return counter.n + "|" + seq.join(",");
}

async function asyncFromSyncTamperBefore(tamper) {
    const counter = { n: 0 };
    if (tamper)
        armReadCounter(counter);
    const seq = [];
    for await (const x of [1, 2, 3])
        seq.push(x);
    if (tamper)
        restoreCtor();
    return counter.n + "|" + seq.join(",");
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount;
    for (let i = 0; i < N; i++) {
        assert((await asyncGeneratorTamperBefore(false)) === "0|1,2,3", "warm async-gen " + i);
        assert((await asyncFromSyncTamperBefore(false)) === "0|1,2,3", "warm async-from-sync " + i);
    }

    assert((await asyncGeneratorTamperBefore(true)) === "5|1,2,3", "async-gen tamper-before");
    assert((await asyncFromSyncTamperBefore(true)) === "5|1,2,3", "async-from-sync tamper-before");
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

restoreCtor();
if (error)
    throw error;
assert(done, "async main() did not complete");
