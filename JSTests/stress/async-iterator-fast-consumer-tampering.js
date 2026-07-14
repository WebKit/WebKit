// Regression tests for the for-await fast consumer under tampering of the objects its correctness leans
// on. All expected results were verified against V8.
//   - Tampering %AsyncGeneratorPrototype%.next breaks the pristine-next check, so op_async_iterator_open
//     must NOT install the fast sentinel and op_async_iterator_next must call the (tampered) next.
//   - The fast consumer settles the consumer's resume via resolveWithInternalMicrotask, which must keep
//     resolvePromise's spec thenable check. With Object.prototype.then defined, the iterator-result
//     {value,done} objects become thenable and must be run through that check, exactly as a real Promise
//     settlement would -- otherwise the fast path would observably diverge from spec.
//   - for-await must not route through a user-tampered Promise.prototype.then.
// Warmed to reach the upper JIT tiers.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

const AsyncGeneratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf((async function* () {})()));

// %AsyncGeneratorPrototype%.next wrapped: for-await must invoke it (generic real-call path), once per
// yield plus once for the terminating { done: true }.
async function tamperNext() {
    const original = AsyncGeneratorPrototype.next;
    let calls = 0;
    AsyncGeneratorPrototype.next = function (...args) { calls++; return original.apply(this, args); };
    try {
        async function* g() { yield 1; yield 2; yield 3; }
        let sum = 0;
        for await (const x of g())
            sum += x;
        assert(sum === 6, "tamperNext sum=" + sum);
        assert(calls === 4, "tamperNext calls=" + calls); // 3 yields + 1 final next
    } finally {
        AsyncGeneratorPrototype.next = original;
    }
}

// A tampered Promise.prototype.then must not affect for-await (it uses internal microtasks, not the
// user-visible then), and iteration must still be correct.
async function tamperPromiseThen() {
    const original = Promise.prototype.then;
    Promise.prototype.then = function (...args) { return original.apply(this, args); };
    try {
        async function* g() { yield 10; yield 20; }
        let sum = 0;
        for await (const x of g())
            sum += x;
        assert(sum === 30, "tamperPromiseThen sum=" + sum);
    } finally {
        Promise.prototype.then = original;
    }
}

// Object.prototype.then defined: iterator-result objects become thenable. The fast consumer must run
// them through the thenable check (observed via irThen) and iteration must still complete correctly.
async function tamperObjectThen() {
    let irThen = 0;
    Object.defineProperty(Object.prototype, "then", {
        configurable: true, writable: true,
        value: function (onFulfilled) {
            if (this && typeof this === "object" && "value" in this && "done" in this) {
                irThen++;
                // Resolve with a non-thenable (null-proto) copy so iteration can complete.
                const copy = Object.create(null);
                copy.value = this.value;
                copy.done = this.done;
                onFulfilled(copy);
                return;
            }
            onFulfilled(this);
        },
    });
    let sum = 0;
    try {
        async function* g() { yield 100; yield 200; }
        for await (const x of g())
            sum += x;
    } finally {
        delete Object.prototype.then;
    }
    assert(sum === 300, "tamperObjectThen sum=" + sum);
    assert(irThen > 0, "tamperObjectThen expected iterator results to hit the thenable check");
}

let done = false;
let error = null;

async function main() {
    const N = testLoopCount; // warm hot enough to reach the upper JIT tiers.
    for (let i = 0; i < N; i++) {
        await tamperNext();
        await tamperPromiseThen();
        await tamperObjectThen();
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
