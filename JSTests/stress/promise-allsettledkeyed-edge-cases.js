//@ requireOptions("--usePromiseAllKeyed=1")

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

// The resolve-element and reject-element closures share the [[AlreadyCalled]] record: whichever is
// called first wins, and every later call through either function is ignored.
{
    let onFulfilledFunction;
    let onRejectedFunction;
    let input = {
        key: {
            then(onFulfilled, onRejected) {
                onFulfilledFunction = onFulfilled;
                onRejectedFunction = onRejected;
                onFulfilled("first");
                onRejected("second"); // Ignored.
                onFulfilled("third"); // Ignored.
            }
        },
    };
    let results = [];
    Promise.allSettledKeyed(input).then((value) => { results.push(value); });
    drainMicrotasks();
    shouldBe(results.length, 1);
    checkFulfilledEntry(results[0].key, "first");
    shouldBe(typeof onFulfilledFunction, "function");
    shouldBe(typeof onRejectedFunction, "function");
    shouldBe(onFulfilledFunction !== onRejectedFunction, true);
    shouldBe(onFulfilledFunction.length, 1);
    shouldBe(onRejectedFunction.length, 1);
    shouldBe(onFulfilledFunction.name, "");
    shouldBe(onRejectedFunction.name, "");
}

// Reject-first through the shared record.
{
    let results = [];
    let input = {
        key: {
            then(onFulfilled, onRejected) {
                onRejected("reason");
                onFulfilled("ignored");
            }
        },
    };
    Promise.allSettledKeyed(input).then((value) => { results.push(value); });
    drainMicrotasks();
    shouldBe(results.length, 1);
    checkRejectedEntry(results[0].key, "reason");
}

// Abrupt completions during iteration (getter throw) reject the combined promise: allSettledKeyed
// only converts promise rejections into entries, not walk-time exceptions.
{
    let reason;
    let input = { a: Promise.resolve("a") };
    Object.defineProperty(input, "b", { get() { throw new TypeError("boom"); }, enumerable: true });
    Promise.allSettledKeyed(input).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(String(reason), "TypeError: boom");
}

// ownKeys trap throwing rejects the combined promise.
{
    let reason;
    Promise.allSettledKeyed(new Proxy({}, { ownKeys() { throw new Error("ownKeys"); } })).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(String(reason), "Error: ownKeys");
}

// Index keys mixed with named keys, settled in adverse order, with rejections mixed in.
{
    let controls = { };
    let input = { };
    for (let key of ["n1", 4, "n0", 0])
        input[key] = new Promise((resolve, reject) => { controls[key] = { resolve, reject }; });
    let result;
    Promise.allSettledKeyed(input).then((value) => { result = value; });
    controls[0].reject("r0");
    controls.n1.resolve("vn1");
    controls[4].resolve("v4");
    controls.n0.reject("rn0");
    drainMicrotasks();
    shouldBeArray(Reflect.ownKeys(result), ["0", "4", "n1", "n0"]);
    checkRejectedEntry(result[0], "r0");
    checkFulfilledEntry(result[4], "v4");
    checkFulfilledEntry(result.n1, "vn1");
    checkRejectedEntry(result.n0, "rn0");
}

// A dictionary-sized key set with alternating outcomes lands every entry in the right slot.
{
    let count = 150;
    let input = { };
    let controls = [];
    for (let i = 0; i < count; ++i)
        input["k" + i] = new Promise((resolve, reject) => { controls.push({ resolve, reject }); });
    let result;
    Promise.allSettledKeyed(input).then((value) => { result = value; });
    for (let i = count - 1; i >= 0; --i) {
        if (i % 2)
            controls[i].reject("r" + i);
        else
            controls[i].resolve("v" + i);
    }
    drainMicrotasks();
    let keys = Reflect.ownKeys(result);
    shouldBe(keys.length, count);
    for (let i = 0; i < count; ++i) {
        shouldBe(keys[i], "k" + i);
        if (i % 2)
            checkRejectedEntry(result["k" + i], "r" + i);
        else
            checkFulfilledEntry(result["k" + i], "v" + i);
    }
}

// The combined promise never rejects even when every element rejects.
{
    let result;
    let rejected = false;
    Promise.allSettledKeyed({
        a: Promise.reject("ra"),
        b: Promise.reject("rb"),
    }).then((value) => { result = value; }, () => { rejected = true; });
    drainMicrotasks();
    shouldBe(rejected, false);
    checkRejectedEntry(result.a, "ra");
    checkRejectedEntry(result.b, "rb");
}
