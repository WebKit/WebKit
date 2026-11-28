// A deferred SafePromiseResolve calls "then" from the deferred job itself, so a thenable settles the
// promise at the same microtask tick as an ordinary resolve with it. Only resolutions that need no
// further job (a getter returning a non-callable, a Proxy without "then") take one extra tick.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

function measure(settle) {
    const { promise, resolve } = Promise.withResolvers();
    let tickAtSettlement = -1;
    let tick = 0;
    promise.then(() => { tickAtSettlement = tick; }, () => { tickAtSettlement = tick; });
    settle(promise, resolve);
    let pending = Promise.resolve();
    for (let i = 0; i < 10; ++i)
        pending = pending.then(() => { ++tick; });
    drainMicrotasks();
    shouldBe(tickAtSettlement >= 0, true);
    return tickAtSettlement;
}

function sameTick(makeValue) {
    const ordinary = measure((promise, resolve) => resolve(makeValue()));
    const safe = measure((promise) => $vm.safeResolvePromise(promise, makeValue()));
    shouldBe(safe, ordinary);
}

function oneMoreTick(makeValue) {
    const ordinary = measure((promise, resolve) => resolve(makeValue()));
    const safe = measure((promise) => $vm.safeResolvePromise(promise, makeValue()));
    shouldBe(safe, ordinary + 1);
}

sameTick(() => 42);
sameTick(() => ({ }));
sameTick(() => ({ then: 42 }));
sameTick(() => Object.defineProperty({ }, "then", { set(v) { } }));
sameTick(() => ({ then(onFulfilled) { onFulfilled("thenable"); } }));
sameTick(() => ({ then(onFulfilled, onRejected) { onRejected("thenable"); } }));
sameTick(() => ({ get then() { return function (onFulfilled) { onFulfilled("getter"); }; } }));
sameTick(() => Promise.resolve("fulfilled"));
sameTick(() => Promise.reject("rejected"));
sameTick(() => {
    const promise = Promise.resolve("own-then");
    promise.then = Promise.prototype.then;
    return promise;
});
sameTick(() => {
    class SubPromise extends Promise { }
    return SubPromise.resolve("subclass");
});
sameTick(() => new Proxy({ }, { get(target, name) { return name === "then" ? (r) => r("proxy") : undefined; } }));

oneMoreTick(() => ({ get then() { return undefined; } }));
oneMoreTick(() => new Proxy({ }, { }));
oneMoreTick(() => ({ get then() { throw new Error("getter"); } }));

// A module namespace object always defers, even though it has no "then".
{
    let namespace;
    import("./resources/empty.js").then((value) => { namespace = value; });
    drainMicrotasks();
    shouldBe(typeof namespace, "object");
    oneMoreTick(() => namespace);
}
