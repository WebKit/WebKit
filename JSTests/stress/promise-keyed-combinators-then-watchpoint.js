//@ requireOptions("--usePromiseAllKeyed=1")

// Fires the Promise.prototype.then watchpoint before calling the keyed combinators. This disables
// canSkipIntermediatePromise and the internal-microtask reaction path, so every element must go
// through the observable |then| call with real resolve-element functions. Must stay a separate
// file: once the watchpoint fires it never rearms in this VM.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

const originalThen = Promise.prototype.then;
let thenCalls = 0;
Promise.prototype.then = function (onFulfilled, onRejected) {
    thenCalls++;
    return originalThen.call(this, onFulfilled, onRejected);
};

// allKeyed: every element (including non-thenable values) now reaches |then| with element functions.
{
    let result;
    let before = thenCalls;
    let returned = Promise.allKeyed({ a: Promise.resolve(1), b: 2, c: Promise.resolve(3) });
    let afterCombinator = thenCalls;
    originalThen.call(returned, (value) => { result = value; });
    drainMicrotasks();
    shouldBe(afterCombinator - before, 3);
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBeArray(Reflect.ownKeys(result), ["a", "b", "c"]);
    shouldBe(result.a, 1);
    shouldBe(result.b, 2);
    shouldBe(result.c, 3);
}

// allKeyed: rejection still wins through the overridden then.
{
    let reason;
    let returned = Promise.allKeyed({ a: Promise.resolve(1), b: Promise.reject("boom") });
    originalThen.call(returned, undefined, (error) => { reason = error; });
    drainMicrotasks();
    shouldBe(reason, "boom");
}

// allSettledKeyed: fulfilled and rejected entries are still built correctly.
{
    let result;
    let before = thenCalls;
    let returned = Promise.allSettledKeyed({ ok: Promise.resolve("v"), ng: Promise.reject("r"), plain: 5 });
    let afterCombinator = thenCalls;
    originalThen.call(returned, (value) => { result = value; });
    drainMicrotasks();
    shouldBe(afterCombinator - before, 3);
    shouldBe(result.ok.status, "fulfilled");
    shouldBe(result.ok.value, "v");
    shouldBe(result.ng.status, "rejected");
    shouldBe(result.ng.reason, "r");
    shouldBe(result.plain.status, "fulfilled");
    shouldBe(result.plain.value, 5);
}

// A hostile then that swallows the element functions leaves the combined promise pending forever
// (spec behavior) instead of resolving or crashing.
{
    let settledState = "pending";
    Promise.prototype.then = function () { /* swallow */ };
    let returned = Promise.allKeyed({ a: Promise.resolve(1) });
    originalThen.call(returned, () => { settledState = "fulfilled"; }, () => { settledState = "rejected"; });
    drainMicrotasks();
    shouldBe(settledState, "pending");
    Promise.prototype.then = originalThen;
}
