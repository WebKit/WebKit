//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${JSON.stringify(b)} but got ${JSON.stringify(a)}`);
}

// AddDisposableResource step 1.a returns unused only when hint is sync-dispose.
// For async-dispose with V = null/undefined, CreateDisposableResource produces
// { undefined, undefined, async-dispose }, which causes DisposeResources to
// set needsAwait=true (step 3.f), resulting in one trailing Await(undefined)
// (step 6) if no other resource was awaited.

// Measure relative timing: disposeAsync with use(null) resolves later than empty.
function probe(setup) {
    const events = [];
    const stack = new AsyncDisposableStack();
    setup(stack);
    stack.disposeAsync().then(() => events.push("D"));
    Promise.resolve()
        .then(() => events.push("1"))
        .then(() => events.push("2"))
        .then(() => events.push("3"));
    drainMicrotasks();
    return events.join("");
}

const empty = probe(() => {});
const one = probe(s => s.use(null));
const two = probe(s => { s.use(null); s.use(null); });
const three = probe(s => { s.use(null); s.use(null); s.use(null); });
const undef = probe(s => s.use(undefined));

// Empty stack: finish() immediately, no Await.
// use(null): loop sets needsAwait, finish via @promiseResolve(undefined).then → one tick later.
shouldBe(empty !== one, true);

// Multiple use(null) → only one Await(undefined) (needsAwait is boolean).
shouldBe(one, two);
shouldBe(two, three);

// use(undefined) behaves the same as use(null).
shouldBe(one, undef);

// use(null) mixed with a real resource: the real resource's Await sets
// hasAwaited=true, so the trailing Await(undefined) is skipped.
{
    let disposed = false;
    const log = [];
    const stack = new AsyncDisposableStack();
    stack.use(null);
    stack.use({ [Symbol.asyncDispose]() { disposed = true; } });
    stack.use(null);
    stack.disposeAsync().then(() => log.push("D"));
    Promise.resolve()
        .then(() => log.push("1"))
        .then(() => log.push("2"))
        .then(() => log.push("3"));
    drainMicrotasks();
    shouldBe(disposed, true);
    // The real resource already provides a microtask boundary; timing should not
    // be delayed by an additional Await(undefined).
    const realOnly = probe(s => s.use({ [Symbol.asyncDispose]() {} }));
    shouldBe(log.join(""), realOnly);
}

// use(null) + defer: defer's closure has a defined method, so hasAwaited
// becomes true and the trailing Await is skipped.
{
    const stack = new AsyncDisposableStack();
    let deferred = false;
    stack.use(null);
    stack.defer(() => { deferred = true; });
    let resolved = false;
    stack.disposeAsync().then(() => { resolved = true; });
    drainMicrotasks();
    shouldBe(deferred, true);
    shouldBe(resolved, true);
}

// use(null) + error in another resource: error still propagates correctly.
{
    const stack = new AsyncDisposableStack();
    stack.use(null);
    stack.use({ [Symbol.asyncDispose]() { throw new Error("fail"); } });
    stack.use(null);
    let caught;
    stack.disposeAsync().catch(e => { caught = e; });
    drainMicrotasks();
    shouldBe(caught.message, "fail");
}

// Sync DisposableStack.use(null) is still a no-op (existing behavior).
{
    const stack = new DisposableStack();
    stack.use(null);
    stack.use(undefined);
    stack.dispose();
    shouldBe(stack.disposed, true);
}
