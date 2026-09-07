//@ requireOptions("--usePromiseAllKeyed=1")

// Exercises the generic (non-%Promise% constructor) path of Promise.allSettledKeyed.
// These tests do not fire any global watchpoints.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

function checkFulfilledEntry(entry, expectedValue) {
    shouldBe(Object.getPrototypeOf(entry), Object.prototype);
    shouldBeArray(Reflect.ownKeys(entry), ["status", "value"]);
    shouldBe(entry.status, "fulfilled");
    shouldBe(entry.value, expectedValue);
}

function checkRejectedEntry(entry, expectedReason) {
    shouldBe(Object.getPrototypeOf(entry), Object.prototype);
    shouldBeArray(Reflect.ownKeys(entry), ["status", "reason"]);
    shouldBe(entry.status, "rejected");
    shouldBe(entry.reason, expectedReason);
}

// Subclass end-to-end.
{
    class MyPromise extends Promise { }
    let returned = MyPromise.allSettledKeyed({ ok: Promise.resolve("ok"), ng: Promise.reject("ng"), plain: 7 });
    shouldBe(returned instanceof MyPromise, true);
    let result;
    returned.then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBeArray(Reflect.ownKeys(result), ["ok", "ng", "plain"]);
    checkFulfilledEntry(result.ok, "ok");
    checkRejectedEntry(result.ng, "ng");
    checkFulfilledEntry(result.plain, 7);
}

// Custom constructor: capability resolve receives the keyed result; constructor.resolve is looked
// up once and called once per enumerable key.
{
    let resolveGets = 0;
    let resolveCalls = 0;
    let capabilityResolve;
    function CustomPromise(executor) {
        executor((value) => { capabilityResolve = value; }, () => { });
    }
    Object.defineProperty(CustomPromise, "resolve", {
        configurable: true,
        get() {
            resolveGets++;
            return function (value) {
                resolveCalls++;
                return Promise.resolve(value);
            };
        },
    });
    let input = { ok: Promise.resolve(1), ng: Promise.reject(2) };
    Object.defineProperty(input, "hidden", { value: 3, enumerable: false });
    Promise.allSettledKeyed.call(CustomPromise, input);
    drainMicrotasks();
    shouldBe(resolveGets, 1);
    shouldBe(resolveCalls, 2);
    shouldBe(Object.getPrototypeOf(capabilityResolve), null);
    checkFulfilledEntry(capabilityResolve.ok, 1);
    checkRejectedEntry(capabilityResolve.ng, 2);
}

// Slow-path element function pair shares the [[AlreadyCalled]] record.
{
    let captured = [];
    let capabilityResolve;
    function CollectingPromise(executor) {
        executor((value) => { capabilityResolve = value; }, () => { });
    }
    CollectingPromise.resolve = function (value) { return value; }; // Pass thenables through.

    let input = {
        key: { then(onFulfilled, onRejected) { captured.push(onFulfilled, onRejected); } },
    };
    Promise.allSettledKeyed.call(CollectingPromise, input);
    shouldBe(captured.length, 2);
    let [onFulfilled, onRejected] = captured;
    shouldBe(onFulfilled !== onRejected, true);
    shouldBe(onFulfilled.length, 1);
    shouldBe(onRejected.length, 1);
    shouldBe(onFulfilled.name, "");
    shouldBe(onRejected.name, "");

    onRejected("reason");
    onFulfilled("ignored"); // [[AlreadyCalled]] is shared with the reject side.
    drainMicrotasks();
    checkRejectedEntry(capabilityResolve.key, "reason");
}

// Non-callable constructor.resolve rejects with a TypeError.
{
    let rejected;
    function CustomPromise(executor) {
        executor(() => { }, (error) => { rejected = error; });
    }
    CustomPromise.resolve = null;
    Promise.allSettledKeyed.call(CustomPromise, { a: 1 });
    drainMicrotasks();
    shouldBe(String(rejected), "TypeError: Promise resolve is not a function");
}
