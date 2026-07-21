// Verifies the per-producer cached iterator-result-object reuse optimization (317587@main) stays
// correct when the for-await *driver* is a top-level-await (TLA) module. module-fast-drive routes
// module for-await through the fast consumer path (op_async_iterator_open fast sentinel ->
// op_async_iterator_next driver branch -> AsyncGeneratorDriverResume with the module record as the
// driver), so the async generator producer's cached result object is now handed to a module driver.
// That reuse is only sound while the object cannot be observed; these checks confirm both the
// non-observable fast path (correct values across many yields) and the observable-`then` fallback
// (distinct, unmutated results) for the module driver.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

let error = null;
let pending = 0;

function track(promise, onValue) {
    pending++;
    promise.then((v) => { onValue(v); pending--; }, (e) => { error = e; pending--; });
}

// Sequence the two scenarios: the fast (no observable then) module must fully settle before the
// aliasing module installs the global `then` getter, otherwise the fast module's own iterator
// results would be captured too.
track(
    import("./resources/async-iterator-tla-cached-fast.js").then(() => {
        const r = globalThis.__tlaCachedFast;
        shouldBe(r.length, 500);
        for (let i = 0; i < 500; ++i)
            shouldBe(r[i], i);
        return import("./resources/async-iterator-tla-cached-aliasing.js");
    }).then(() => {
        const { delivered, captured } = globalThis.__tlaCachedAliasing;

        // The module for-await must still deliver the correct values.
        shouldBe(delivered.join(","), "10,20,30");

        // Every captured iterator result must be a distinct object (no reused/aliased cached object).
        shouldBe(new Set(captured.map(c => c.result)).size, captured.length);

        // The value observed at capture time must not have been mutated afterwards by reuse.
        for (const c of captured)
            shouldBe(c.result.value, c.value);

        // The three value-carrying results must retain 10/20/30.
        const values = captured.filter(c => c.done === false).map(c => c.value);
        shouldBe(values.join(","), "10,20,30");

        return import("./resources/async-iterator-tla-cached-mid-lifetime.js");
    }).then(() => {
        const { delivered, captured } = globalThis.__tlaCachedMidLifetime;

        // The module for-await must still deliver every value across the watchpoint transition.
        shouldBe(delivered.join(","), "a,b,c,d");

        // The cached object created while non-observable must not leak once `then` is defined:
        // every captured result is a distinct object.
        shouldBe(new Set(captured).size, captured.length);
    }),
    () => { });

drainMicrotasks();

if (error)
    throw error;
shouldBe(pending, 0);
