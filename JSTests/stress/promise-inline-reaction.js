// Stress test for the inline-reaction optimization in JSPromise: when a pending
// promise's only reaction is an internal microtask with an undefined result
// promise (the common `await pending_promise` case), the reaction is stored
// inline in the promise instead of allocating a JSSlimPromiseReaction. This test
// exercises the spill paths and edge cases that could break if the inline
// representation, the spill logic, or the JSPromise::Flags layout changes.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`${msg || ""}: expected ${expected} but got ${actual}`);
}

function shouldThrow(fn, expected, msg) {
    let threw = false;
    try { fn(); } catch (e) { threw = true; shouldBe(String(e), expected, msg); }
    if (!threw) throw new Error(`${msg || ""}: expected to throw but did not`);
}

let failures = [];
let pendingTests = 0;
function asyncTest(name, fn) {
    pendingTests++;
    fn().then(() => {
        pendingTests--;
    }, (e) => {
        pendingTests--;
        failures.push(`${name}: ${e.stack || e}`);
    });
}

// 1. Basic: await on a pending promise (sets the inline reaction).
asyncTest("basic-await-pending", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    Promise.resolve().then(() => resolve(42));
    shouldBe(await p, 42);
});

// 2. await + .then() on the same pending promise.
//    The .then() must spill the inline reaction into a JSSlimPromiseReaction
//    chain. Both consumers must see the value, in registration order.
asyncTest("spill-then-after-await-attached", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    let order = [];
    const awaiter = (async () => {
        order.push("await:" + await p);
    })();
    p.then(v => order.push("then:" + v));
    Promise.resolve().then(() => resolve(7));
    await awaiter;
    await Promise.resolve(); // drain the .then() callback
    shouldBe(order.join(","), "await:7,then:7");
});

// 3. .then() before await (existing reaction chain, no inline).
asyncTest("then-before-await", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    let order = [];
    p.then(v => order.push("then:" + v));
    const awaiter = (async () => { order.push("await:" + await p); })();
    Promise.resolve().then(() => resolve(8));
    await awaiter;
    shouldBe(order.join(","), "then:8,await:8");
});

// 4. Many .then() after await spill: ensure the chain order is preserved.
asyncTest("spill-many-thens", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    let order = [];
    const awaiter = (async () => { order.push("a"); await p; order.push("a-done"); })();
    for (let i = 0; i < 5; ++i)
        p.then(() => order.push("t" + i));
    Promise.resolve().then(() => resolve());
    await awaiter;
    await Promise.resolve();
    shouldBe(order.join(","), "a,a-done,t0,t1,t2,t3,t4");
});

// 5. Two awaits on the same pending promise: the second await must spill.
asyncTest("two-awaits-same-promise", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    let order = [];
    const a1 = (async () => { order.push("a1:" + await p); })();
    const a2 = (async () => { order.push("a2:" + await p); })();
    Promise.resolve().then(() => resolve(99));
    await a1;
    await a2;
    shouldBe(order.join(","), "a1:99,a2:99");
});

// 6. Reject path: the inline reaction must trigger the catch handler.
asyncTest("inline-reaction-reject", async () => {
    let reject;
    const p = new Promise((r, j) => reject = j);
    Promise.resolve().then(() => reject(new Error("boom")));
    let caught;
    try { await p; } catch (e) { caught = e.message; }
    shouldBe(caught, "boom");
});

// 7. Reject after spilling: both the awaiter and the .catch() must observe.
asyncTest("spill-then-reject", async () => {
    let reject;
    const p = new Promise((r, j) => reject = j);
    let order = [];
    const awaiter = (async () => {
        try { await p; } catch (e) { order.push("await-caught:" + e.message); }
    })();
    p.catch(e => order.push("catch:" + e.message));
    Promise.resolve().then(() => reject(new Error("err")));
    await awaiter;
    await Promise.resolve();
    shouldBe(order.join(","), "await-caught:err,catch:err");
});

// 8. Already-fulfilled promise: must not take the inline path. await still works.
asyncTest("fulfilled-promise-await", async () => {
    const p = Promise.resolve(123);
    shouldBe(await p, 123);
});

// 9. Already-rejected promise: must not take the inline path.
asyncTest("rejected-promise-await", async () => {
    const p = Promise.reject(new Error("pre")).then(() => {}, e => "handled:" + e.message);
    shouldBe(await p, "handled:pre");
});

// 10. Promise piped from another (pipeFrom). The result promise of the inline
//     reaction is non-undefined here, so it must NOT use the inline path.
asyncTest("promise-resolve-thenable", async () => {
    let resolve;
    const inner = new Promise(r => resolve = r);
    const outer = Promise.resolve(inner); // pipeFrom path
    Promise.resolve().then(() => resolve(55));
    shouldBe(await outer, 55);
});

// 11. for await...of with async generator (the original motivation).
asyncTest("for-await-of", async () => {
    async function* gen(n) {
        for (let i = 0; i < n; ++i)
            yield i;
    }
    let sum = 0;
    for await (const x of gen(100))
        sum += x;
    shouldBe(sum, 4950);
});

// 12. async generator return / throw drive the inline reaction too.
asyncTest("async-gen-return-throw", async () => {
    async function* g() { try { yield 1; yield 2; } finally { /* */ } }
    let it = g();
    shouldBe((await it.next()).value, 1);
    shouldBe((await it.return(99)).value, 99);

    it = g();
    await it.next();
    let caught;
    try { await it.throw(new Error("agg")); } catch (e) { caught = e.message; }
    shouldBe(caught, "agg");
});

// 13. GC pressure during spill: the inline reaction's context cell must survive
//     the JSSlimPromiseReaction allocation that the spill triggers.
asyncTest("gc-pressure-during-spill", async () => {
    for (let round = 0; round < 100; ++round) {
        let resolve;
        const p = new Promise(r => resolve = r);
        const awaiter = (async () => { return await p; })();
        // Allocate a lot to encourage GC right around the spill.
        let garbage = [];
        for (let i = 0; i < 100; ++i) garbage.push({ a: i, b: i, c: i });
        p.then(() => {});  // spill
        garbage = null;
        Promise.resolve().then(() => resolve(round));
        shouldBe(await awaiter, round);
    }
});

// 14. Heavy async generator stress with GC.
asyncTest("async-gen-gc-stress", async () => {
    async function* g() {
        for (let i = 0; i < 1000; ++i) {
            yield { v: i };
        }
    }
    let count = 0;
    for await (const o of g()) {
        if (o.v !== count) throw new Error("mismatch");
        count++;
    }
    shouldBe(count, 1000);
});

// 15. Microtask ordering: the inline reaction must produce the same observable
//     ordering as a heap-allocated JSSlimPromiseReaction would.
asyncTest("microtask-ordering", async () => {
    let order = [];
    let resolve;
    const p = new Promise(r => resolve = r);
    const awaiter = (async () => { await p; order.push("A"); })();
    Promise.resolve().then(() => order.push("B"));
    Promise.resolve().then(() => { resolve(); order.push("C"); });
    Promise.resolve().then(() => order.push("D"));
    await awaiter;
    await Promise.resolve();
    await Promise.resolve();
    // B and D fire before resolve(); A fires after resolve.
    shouldBe(order.join(","), "B,C,D,A");
});

// 16. Spill ordering: registering .then() after the promise is already settled
//     must not see a stale inline reaction.
asyncTest("then-after-settle", async () => {
    let resolve;
    const p = new Promise(r => resolve = r);
    let order = [];
    const awaiter = (async () => { await p; order.push("A"); })();
    Promise.resolve().then(() => resolve());
    await awaiter;
    p.then(() => order.push("late"));
    await Promise.resolve();
    shouldBe(order.join(","), "A,late");
});

// 17. dynamic import resolution path uses internal microtask reactions too.
asyncTest("dynamic-import", async () => {
    try {
        await import("does-not-exist-xyzzy");
        throw new Error("should not resolve");
    } catch (e) {
        // Any error type is fine; the point is the await on a pending
        // import promise didn't crash or misbehave.
    }
});

// 18. Promise with subclass thenable should not take the inline path.
asyncTest("custom-thenable-await", async () => {
    let resolve;
    const inner = new Promise(r => resolve = r);
    const thenable = {
        then(onFulfill) { inner.then(v => onFulfill(v * 2)); }
    };
    Promise.resolve().then(() => resolve(10));
    shouldBe(await thenable, 20);
});

// Drain the loop and report.
function finish() {
    if (pendingTests > 0) {
        Promise.resolve().then(finish);
        return;
    }
    if (failures.length)
        throw new Error("Failures:\n" + failures.join("\n"));
}
finish();
