// SafePromiseResolve with a value whose resolution could run user code defers every such step to a
// microtask, so Get(resolution, "then") and the call to "then" never happen on the caller's stack.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected ${String(expected)} but got ${String(actual)}`);
}

function observe(promise) {
    let result = null;
    promise.then((value) => { result = { value }; }, (reason) => { result = { reason }; });
    return () => result;
}

// Callable own "then" data property.
{
    const { promise } = Promise.withResolvers();
    const log = [];
    const thenable = {
        then(onFulfilled, onRejected) {
            log.push("then");
            shouldBe(this, thenable);
            shouldBe(typeof onFulfilled, "function");
            shouldBe(typeof onRejected, "function");
            onFulfilled(42);
        }
    };
    $vm.safeResolvePromise(promise, thenable);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "then");
    shouldBe(result().value, 42);
}

// "then" getter runs in the job, followed immediately by the call.
{
    const { promise } = Promise.withResolvers();
    const log = [];
    const thenable = {
        get then() {
            log.push("get");
            return function (onFulfilled) {
                log.push("call");
                onFulfilled("from-getter");
            };
        }
    };
    $vm.safeResolvePromise(promise, thenable);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "get,call");
    shouldBe(result().value, "from-getter");
}

// "then" getter returning a non-callable fulfills with the object, a tick later.
{
    const { promise } = Promise.withResolvers();
    let getterCalls = 0;
    const object = { get then() { ++getterCalls; return 42; } };
    $vm.safeResolvePromise(promise, object);
    shouldBe(getterCalls, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(getterCalls, 1);
    shouldBe(result().value, object);
}

// The motivating case: a getter installed on Object.prototype must not run synchronously.
{
    let getterCalls = 0;
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            ++getterCalls;
            return undefined;
        }
    });
    try {
        const { promise } = Promise.withResolvers();
        const dictionary = { value: "chunk", done: false };
        $vm.safeResolvePromise(promise, dictionary);
        shouldBe(getterCalls, 0);
        const result = observe(promise);
        drainMicrotasks();
        shouldBe(getterCalls, 1);
        shouldBe(result().value, dictionary);
    } finally {
        delete Object.prototype.then;
    }
}

// Callable "then" installed on Object.prototype.
{
    const log = [];
    Object.prototype.then = function (onFulfilled) {
        delete Object.prototype.then;
        log.push("then");
        onFulfilled("hijacked");
    };
    try {
        const { promise } = Promise.withResolvers();
        $vm.safeResolvePromise(promise, { });
        shouldBe(log.length, 0);
        const result = observe(promise);
        drainMicrotasks();
        shouldBe(log.join(), "then");
        shouldBe(result().value, "hijacked");
    } finally {
        delete Object.prototype.then;
    }
}

// Inherited "then".
{
    const { promise } = Promise.withResolvers();
    const log = [];
    class Thenable {
        then(onFulfilled) {
            log.push("then");
            onFulfilled("proto-value");
        }
    }
    $vm.safeResolvePromise(promise, new Thenable);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "then");
    shouldBe(result().value, "proto-value");
}

// A Proxy always defers, even when it would report no "then".
{
    const { promise } = Promise.withResolvers();
    const log = [];
    const proxy = new Proxy({ }, {
        get(target, name) {
            log.push("get:" + String(name));
            if (name === "then") {
                return function (onFulfilled) {
                    log.push("then");
                    onFulfilled("proxy-value");
                };
            }
            return target[name];
        },
        getOwnPropertyDescriptor(target, name) {
            log.push("gopd:" + String(name));
            return Reflect.getOwnPropertyDescriptor(target, name);
        },
        getPrototypeOf(target) {
            log.push("getPrototypeOf");
            return Reflect.getPrototypeOf(target);
        },
    });
    $vm.safeResolvePromise(promise, proxy);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "get:then,then");
    shouldBe(result().value, "proxy-value");
}

{
    const { promise } = Promise.withResolvers();
    const proxy = new Proxy({ }, { });
    $vm.safeResolvePromise(promise, proxy);
    const order = [];
    promise.then(() => order.push("promise"));
    Promise.resolve().then(() => order.push("next"));
    drainMicrotasks();
    shouldBe(order.join(), "next,promise");
}

// A Proxy on the prototype chain defers too.
{
    const { promise } = Promise.withResolvers();
    let trapCalls = 0;
    const object = Object.create(new Proxy({ }, { get() { ++trapCalls; return undefined; } }));
    $vm.safeResolvePromise(promise, object);
    shouldBe(trapCalls, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(trapCalls, 1);
    shouldBe(result().value, object);
}

// A revoked Proxy rejects in the job.
{
    const { promise } = Promise.withResolvers();
    const { proxy, revoke } = Proxy.revocable({ }, { });
    revoke();
    $vm.safeResolvePromise(promise, proxy);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason instanceof TypeError, true);
}

// A throwing getter rejects in the job.
{
    const { promise } = Promise.withResolvers();
    const error = new Error("getter");
    $vm.safeResolvePromise(promise, { get then() { throw error; } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason, error);
}

// A throwing "then" rejects, unless it already resolved.
{
    const { promise } = Promise.withResolvers();
    const error = new Error("then");
    $vm.safeResolvePromise(promise, { then() { throw error; } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason, error);
}

{
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, { then(onFulfilled) { onFulfilled("first"); throw new Error("ignored"); } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "first");
}

// The resolving functions given to "then" resolve recursively, including with further thenables.
{
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, { then(onFulfilled) { onFulfilled({ then(r) { r("nested"); } }); } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "nested");
}

{
    const { promise } = Promise.withResolvers();
    const error = new Error("rejected");
    $vm.safeResolvePromise(promise, { then(onFulfilled, onRejected) { onRejected(error); onFulfilled("ignored"); } });
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason, error);
}

// A real promise, with an observable own "then".
{
    const { promise } = Promise.withResolvers();
    const inner = Promise.resolve("inner-value");
    const log = [];
    const originalThen = Promise.prototype.then;
    Object.defineProperty(inner, "then", {
        value: function (...args) {
            log.push("then");
            return originalThen.apply(this, args);
        },
        writable: true,
        configurable: true,
    });
    $vm.safeResolvePromise(promise, inner);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "then");
    shouldBe(result().value, "inner-value");
}

// Real promises, fulfilled, rejected and pending.
{
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, Promise.resolve("fulfilled"));
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "fulfilled");
}

{
    const { promise } = Promise.withResolvers();
    const error = new Error("rejected");
    $vm.safeResolvePromise(promise, Promise.reject(error));
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().reason, error);
}

{
    const { promise } = Promise.withResolvers();
    const inner = Promise.withResolvers();
    $vm.safeResolvePromise(promise, inner.promise);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result(), null);
    inner.resolve("later");
    drainMicrotasks();
    shouldBe(result().value, "later");
}

// A promise subclass with an observable constructor lookup.
{
    const { promise } = Promise.withResolvers();
    const log = [];
    class SubPromise extends Promise { }
    const inner = SubPromise.resolve("sub");
    Object.defineProperty(inner, "constructor", {
        get() {
            log.push("constructor");
            return SubPromise;
        }
    });
    $vm.safeResolvePromise(promise, inner);
    shouldBe(log.length, 0);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(log.join(), "constructor");
    shouldBe(result().value, "sub");
}

// A promise from another realm.
{
    const other = createGlobalObject();
    const { promise } = Promise.withResolvers();
    $vm.safeResolvePromise(promise, other.Promise.resolve("cross-realm"));
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "cross-realm");
}

// A thenable from another realm.
{
    const other = createGlobalObject();
    const { promise } = Promise.withResolvers();
    const thenable = other.eval("({ then(onFulfilled) { onFulfilled('cross-realm-thenable'); } })");
    $vm.safeResolvePromise(promise, thenable);
    const result = observe(promise);
    drainMicrotasks();
    shouldBe(result().value, "cross-realm-thenable");
}
