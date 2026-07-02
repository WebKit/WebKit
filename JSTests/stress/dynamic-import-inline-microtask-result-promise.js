// Regression test for performPromiseThenWithInternalMicrotask taking the
// inline-reaction fast path when a result JSPromise* is supplied. The result
// promise is stashed in JSPromise's payloadCell and must be recovered in
// settleInlineInternalMicrotask (and spillInlineReaction) so the result
// promise observed by JS resolves/rejects with the right value.
//
// The user-visible path is JSModuleLoader::loadModule wiring up
// ModuleLoadLinkEvaluateSettled — i.e. every dynamic import() call.

var abort = $vm.abort;

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("expected " + String(expected) + ", got " + String(actual));
}

(async function () {
    {
        const m = await import('./import-tests/cocoa.js');
        shouldBe(m.hello(), 42);
        shouldBe(typeof m.Cocoa, 'function');
    }

    // Re-import: a fresh resultPromise is allocated and routed through the
    // inline path on every call, even when the underlying module is cached.
    {
        const a = await import('./import-tests/cocoa.js');
        const b = await import('./import-tests/cocoa.js');
        const c = await import('./import-tests/cocoa.js');
        shouldBe(a, b);
        shouldBe(b, c);
    }

    // Many concurrent imports of the same module: each gets its own pending
    // intermediate promise + inline reaction. If the inline payloadCell were
    // aliased or overwritten across imports, some awaits would see undefined
    // or the wrong namespace.
    {
        const promises = [];
        for (let i = 0; i < 64; ++i)
            promises.push(import('./import-tests/cocoa.js'));
        const results = await Promise.all(promises);
        for (const r of results)
            shouldBe(r.hello(), 42);
    }

    // Concurrent imports of distinct modules — the per-import resultPromise
    // stashed in payloadCell must not leak across imports.
    {
        const [cocoa, should] = await Promise.all([
            import('./import-tests/cocoa.js'),
            import('./import-tests/should.js'),
        ]);
        shouldBe(cocoa.hello(), 42);
        shouldBe(typeof should.shouldBe, 'function');
    }

    // Nested dynamic import (import() invoked from inside an imported module)
    // — exercises the inline path on a different referrer.
    {
        const multiple = await import('./import-tests/multiple.js');
        const inner = await multiple.result();
        shouldBe(typeof inner, 'object');
    }

    // Failure path: the resultPromise must reject with the load error rather
    // than resolve with undefined.
    {
        let err = null;
        try {
            await import('./this-module-does-not-exist.js');
        } catch (e) {
            err = e;
        }
        if (err === null)
            throw new Error('missing module did not reject');
    }

    // Interleave: kick off a failing import and a successful import together;
    // both resultPromises sit in their own payloadCell slot and must settle
    // independently with the right outcome.
    {
        const ok = import('./import-tests/cocoa.js');
        const bad = import('./this-module-does-not-exist.js').then(
            () => { throw new Error('bad import resolved'); },
            e => 'rejected'
        );
        const [okValue, badValue] = await Promise.all([ok, bad]);
        shouldBe(okValue.hello(), 42);
        shouldBe(badValue, 'rejected');
    }
}()).then(() => {}, abort);
