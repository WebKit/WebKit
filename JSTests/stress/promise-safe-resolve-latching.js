// SafePromiseResolve latches the promise's resolving functions before returning, whether or not it
// defers, so any later resolve or reject (including another SafePromiseResolve) is a no-op.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

function observe(promise) {
    let result = null;
    promise.then((value) => { result = { value }; }, (reason) => { result = { reason }; });
    return () => result;
}

// Synchronous path.
{
    const { promise, resolve, reject } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, "first");
    resolve("second");
    reject("third");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "first");
}

// Deferred path: latched while the promise is still pending.
{
    const { promise, resolve, reject } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, { then(onFulfilled) { onFulfilled("from-thenable"); } });
    resolve("racing-resolve");
    reject("racing-reject");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "from-thenable");
}

// Deferred path, resolving functions from the Promise constructor.
{
    let resolve, reject;
    const promise = new Promise((a, b) => { resolve = a; reject = b; });
    $vm.safeResolvePromise(promise, { then(onFulfilled) { onFulfilled("constructor"); } });
    resolve("racing-resolve");
    reject("racing-reject");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "constructor");
}

// First SafePromiseResolve wins.
{
    const { promise } = Promise.withResolvers();
    let secondThenCalled = false;
    $vm.safeResolvePromise(promise, { then(r) { r("first"); } });
    $vm.safeResolvePromise(promise, { then(r) { secondThenCalled = true; r("second"); } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "first");
    shouldBe(secondThenCalled, false);
}

// Already resolved by its resolving functions: SafePromiseResolve does nothing.
{
    const { promise, resolve } = Promise.withResolvers();
    resolve("preset");
    let thenCalled = false;
    $vm.safeResolvePromise(promise, { then(r) { thenCalled = true; r("ignored"); } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "preset");
    shouldBe(thenCalled, false);
}

{
    const { promise, resolve } = Promise.withResolvers();
    resolve({ then(r) { r("thenable-preset"); } });
    $vm.safeResolvePromise(promise, "ignored");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "thenable-preset");
}

{
    const { promise, reject } = Promise.withResolvers();
    reject("rejected");
    $vm.safeResolvePromise(promise, "ignored");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason, "rejected");
}

// Reactions do not run before the deferred job.
{
    const { promise } = Promise.withResolvers();
    let fired = false;
    promise.then(() => { fired = true; });
    $vm.safeResolvePromise(promise, { then(r) { r("eventually"); } });
    shouldBe(fired, false);
    drainMicrotasks();
    shouldBe(fired, true);
}

// A promise cannot be resolved with itself.
{
    const { promise, resolve } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, promise);
    resolve("ignored");
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason instanceof TypeError, true);
}

// Same, when Promise.prototype.then is not callable, so nothing is deferred.
{
    const originalThen = Object.getOwnPropertyDescriptor(Promise.prototype, "then");
    const { promise } = Promise.withResolvers();
    Promise.prototype.then = 42;
    $vm.safeResolvePromise(promise, promise);
    Object.defineProperty(Promise.prototype, "then", originalThen);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason instanceof TypeError, true);
}

// Rejection tracking sees the deferred rejection.
{
    let rejected = [];
    setUnhandledRejectionCallback((promise) => rejected.push(promise));
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, { then(onFulfilled, onRejected) { onRejected("boom"); } });
    drainMicrotasks();
    shouldBe(rejected.length, 1);
    shouldBe(rejected[0], promise);
    setUnhandledRejectionCallback(() => { });
}

{
    let threw = false;
    try {
        $vm.safeResolvePromise({ }, 42);
    } catch (error) {
        threw = error instanceof TypeError;
    }
    shouldBe(threw, true);
}
