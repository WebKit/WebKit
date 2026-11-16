function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${expected}, got ${actual}`);
}

// defer() returns a {promise, resolve, reject} capability without touching the
// constructor's internal machinery beyond what users can observe.
function defer() {
    let resolve, reject;
    let p = new Promise((res, rej) => { resolve = res; reject = rej; });
    return { promise: p, resolve, reject };
}

async function main() {
    // .then(f) single-fulfill handler: takes the inline-child path.
    {
        let counter = 0;
        let d = defer();
        let q = d.promise.then(v => { counter++; return v + 1; });
        d.resolve(10);
        shouldBe(await q, 11);
        shouldBe(counter, 1);
    }

    // .catch(f) single-reject handler: inline-childReject path.
    {
        let caught = null;
        let d = defer();
        let q = d.promise.catch(e => { caught = e; return "handled-" + e; });
        d.reject("oops");
        shouldBe(await q, "handled-oops");
        shouldBe(caught, "oops");
    }

    // Multi-consumer on same pending promise: second .then triggers spill of
    // the first inline-child into a JSSlimPromiseReaction.
    {
        let d = defer();
        let a = d.promise.then(v => v * 2);
        let b = d.promise.then(v => v * 3);
        let c = d.promise.then(v => v * 4);
        d.resolve(5);
        shouldBe(await a, 10);
        shouldBe(await b, 15);
        shouldBe(await c, 20);
    }

    // Two-handler .then(f, g): falls through to JSFullPromiseReaction (not inline).
    {
        let d = defer();
        let q = d.promise.then(x => x + "!", e => "err-" + e);
        d.resolve("r");
        shouldBe(await q, "r!");
    }

    // Deep chain: promise.then(f).then(g).then(h) — each step is inline-child
    // on its parent at the time it's attached (parent pending, no existing
    // reactions). As each parent settles, a new microtask resolves the child.
    {
        let p = Promise.resolve(1);
        let r = p.then(x => x + 1).then(x => x * 10).then(x => x - 3);
        shouldBe(await r, 17);
    }

    // Resolving to an already-fulfilled promise goes through the settled branch
    // of performPromiseThen — no inline-child path.
    {
        let r = Promise.resolve(42);
        shouldBe(await r.then(v => v + 1), 43);
        shouldBe(await r.then(v => v * 2), 84);
    }

    // Resolving to an already-rejected promise: settled-rejected branch.
    {
        let r = Promise.reject("bad");
        r.catch(() => {}); // already handle to avoid unhandled rejection
        try { await r.then(v => "never"); } catch (e) { shouldBe(e, "bad"); }
    }

    // .then() with no callable handlers — reuses the existing inline-internal-microtask
    // form via PromiseResolveWithoutHandlerJob. The resulting promise should mirror.
    {
        let p = Promise.resolve("mirror");
        let q = p.then();
        shouldBe(await q, "mirror");
    }

    // Settling a promise that has an inline-child reaction installed: verify the
    // child promise is correctly resolved from the microtask path.
    {
        let d = defer();
        let c = d.promise.then(v => "child:" + v);
        d.resolve("parent");
        shouldBe(await c, "child:parent");
    }

    // Settling a promise whose inline-child is ChildReject (only onRejected supplied)
    // when parent FULFILLS: the child promise should resolve with the parent's value
    // (pass-through).
    {
        let d = defer();
        let c = d.promise.then(undefined, e => "caught:" + e);
        d.resolve("ok");
        shouldBe(await c, "ok");
    }

    // Settling a promise whose inline-child is ChildFulfill when parent REJECTS:
    // child should propagate rejection.
    {
        let d = defer();
        let c = d.promise.then(v => "fulfilled:" + v);
        c.catch(() => {}); // avoid unhandled-rejection noise
        d.reject("boom");
        try { await c; throw new Error("should have thrown"); }
        catch (e) { shouldBe(e, "boom"); }
    }
}

main().then(
    () => {},
    $vm.abort
);
