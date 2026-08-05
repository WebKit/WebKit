// %AsyncFromSyncIteratorPrototype%.throw with an undefined `throw` method must close the sync iterator via
// IteratorClose and settle the returned promise via IfAbruptRejectPromise -- i.e. REJECT the promise rather than
// escape synchronously -- for every IteratorClose outcome. The async-from-sync wrapper is not user-observable, so
// it is reached through `yield*` over a sync iterable inside an async generator. Behavior and error identity are
// verified against the ECMA-262 %AsyncFromSyncIteratorPrototype%.throw / IteratorClose / GetMethod steps and match
// V8 and SpiderMonkey. Regression test for the wrapper rejecting with the wrong error (or throwing synchronously)
// when the sync iterator has no `throw` and a nullish / non-callable / throwing / non-object-returning `return`.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

// Delegate through `yield*` so the async generator drives the async-from-sync wrapper, then throw() into it and
// report how the returned promise settled. Also assert the sync iterator was left closed (next() reports done).
async function throwInto(makeSyncIterator) {
    const iterable = { [Symbol.iterator]: makeSyncIterator };
    async function* wrapper() { yield* iterable; }
    const iter = wrapper();
    await iter.next();

    let outcome;
    try {
        outcome = { rejected: false, value: await iter.throw(new Error("injected")) };
    } catch (e) {
        outcome = { rejected: true, error: e };
    }

    const after = await iter.next();
    assert(after.done === true, "iterator should be completed after throw");
    assert(after.value === undefined, "completed value should be undefined");
    return outcome;
}

let error = null;
let completed = 0;

async function runCases() {
    // Case 1: no `throw`, no `return`. IteratorClose sees an undefined `return` (nothing to close, `return` is
    // NOT invoked), then the promise rejects with the missing-`throw` TypeError. Pre-fix this incorrectly rejected
    // with "Iterator return method is not callable." because undefined `return` was lumped in with non-callable.
    {
        const o = await throwInto(() => ({ next() { return { value: 1, done: false }; } }));
        assert(o.rejected, "case1 must reject");
        assert(o.error instanceof TypeError, "case1 TypeError");
        assert(o.error.message === "Iterator does not provide a throw method.", "case1 message: " + o.error.message);
    }

    // Case 2: no `throw`, present-but-non-callable `return`. GetMethod(syncIterator, "return") throws a TypeError,
    // which flows through IfAbruptRejectPromise -> the promise REJECTS (it must not throw synchronously).
    {
        const o = await throwInto(() => ({ next() { return { value: 1, done: false }; }, return: 42 }));
        assert(o.rejected, "case2 must reject");
        assert(o.error instanceof TypeError, "case2 TypeError");
        assert(o.error.message === "Iterator return method is not callable.", "case2 message: " + o.error.message);
    }

    // Case 3: no `throw`, a `return` that throws. IteratorClose propagates that abrupt completion and the promise
    // rejects with the exact thrown value (a throwing return() must not escape synchronously either).
    {
        const sentinel = { tag: "sentinel" };
        let returnCalls = 0;
        const o = await throwInto(() => ({
            next() { return { value: 1, done: false }; },
            return() { returnCalls++; throw sentinel; },
        }));
        assert(o.rejected, "case3 must reject");
        assert(o.error === sentinel, "case3 rejected with the thrown value");
        assert(returnCalls === 1, "case3 return called exactly once");
    }

    // Case 4: no `throw`, a `return` that yields a non-object. IteratorClose throws the result-interface TypeError.
    {
        let returnCalls = 0;
        const o = await throwInto(() => ({
            next() { return { value: 1, done: false }; },
            return() { returnCalls++; return 2; },
        }));
        assert(o.rejected, "case4 must reject");
        assert(o.error instanceof TypeError, "case4 TypeError");
        assert(o.error.message === "Iterator result interface is not an object.", "case4 message: " + o.error.message);
        assert(returnCalls === 1, "case4 return called exactly once");
    }

    // Case 5: no `throw`, a `return` that yields a valid object. return() runs and its result is ignored; the
    // promise then rejects with the missing-`throw` TypeError.
    {
        let returnCalls = 0;
        const o = await throwInto(() => ({
            next() { return { value: 1, done: false }; },
            return() { returnCalls++; return { value: 2, done: true }; },
        }));
        assert(o.rejected, "case5 must reject");
        assert(o.error instanceof TypeError, "case5 TypeError");
        assert(o.error.message === "Iterator does not provide a throw method.", "case5 message: " + o.error.message);
        assert(returnCalls === 1, "case5 return called exactly once");
    }
}

async function main() {
    // Warm the async-iterator open/next fast paths across tiers, then re-check the same invariants when hot.
    const N = testLoopCount;
    for (let i = 0; i < N; i++)
        await runCases();
    completed = 1;
}

main().then(() => { }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(completed === 1, "async main() did not complete");
