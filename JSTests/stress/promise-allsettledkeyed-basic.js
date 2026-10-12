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

// Property attributes of Promise.allSettledKeyed.
{
    let descriptor = Object.getOwnPropertyDescriptor(Promise, "allSettledKeyed");
    shouldBe(typeof descriptor.value, "function");
    shouldBe(descriptor.writable, true);
    shouldBe(descriptor.enumerable, false);
    shouldBe(descriptor.configurable, true);
    shouldBe(Promise.allSettledKeyed.length, 1);
    shouldBe(Promise.allSettledKeyed.name, "allSettledKeyed");
}

// Basic behavior: never rejects for promise rejections; records status/value or status/reason per key.
{
    let result;
    let rejected = false;
    Promise.allSettledKeyed({
        a: Promise.resolve(1),
        b: Promise.reject(new Error("b")),
        c: 3,
        d: { then(onFulfilled, onRejected) { onRejected("d-reason"); } },
        e: { then(onFulfilled, onRejected) { onFulfilled("e-value"); } },
    }).then((value) => { result = value; }, () => { rejected = true; });
    drainMicrotasks();
    shouldBe(rejected, false);
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBeArray(Reflect.ownKeys(result), ["a", "b", "c", "d", "e"]);
    checkFulfilledEntry(result.a, 1);
    shouldBeArray(Reflect.ownKeys(result.b), ["status", "reason"]);
    shouldBe(result.b.status, "rejected");
    shouldBe(String(result.b.reason), "Error: b");
    checkFulfilledEntry(result.c, 3);
    checkRejectedEntry(result.d, "d-reason");
    checkFulfilledEntry(result.e, "e-value");
}

// Entry property attributes are plain writable/enumerable/configurable data properties.
{
    let result;
    Promise.allSettledKeyed({ k: 1 }).then((value) => { result = value; });
    drainMicrotasks();
    for (let name of ["status", "value"]) {
        let descriptor = Object.getOwnPropertyDescriptor(result.k, name);
        shouldBe(descriptor.writable, true);
        shouldBe(descriptor.enumerable, true);
        shouldBe(descriptor.configurable, true);
    }
}

// Empty object.
{
    let result;
    Promise.allSettledKeyed({}).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBe(Reflect.ownKeys(result).length, 0);
}

// Result ordering with symbols and index keys, mixed settlement order.
{
    let sym = Symbol("sym");
    let resolvers = { };
    let input = { };
    input.z = new Promise((resolve, reject) => { resolvers.z = { resolve, reject }; });
    input[5] = new Promise((resolve, reject) => { resolvers[5] = { resolve, reject }; });
    input[sym] = new Promise((resolve, reject) => { resolvers[sym] = { resolve, reject }; });
    input.a = new Promise((resolve, reject) => { resolvers.a = { resolve, reject }; });
    input[1] = new Promise((resolve, reject) => { resolvers[1] = { resolve, reject }; });

    let result;
    Promise.allSettledKeyed(input).then((value) => { result = value; });
    resolvers.a.reject("a-reason");
    resolvers[sym].resolve("sym-value");
    resolvers[1].resolve("one");
    resolvers.z.resolve("z-value");
    resolvers[5].reject("five-reason");
    drainMicrotasks();
    let keys = Reflect.ownKeys(result);
    shouldBe(keys.length, 5);
    shouldBe(keys[0], "1");
    shouldBe(keys[1], "5");
    shouldBe(keys[2], "z");
    shouldBe(keys[3], "a");
    shouldBe(keys[4], sym);
    checkFulfilledEntry(result[1], "one");
    checkRejectedEntry(result[5], "five-reason");
    checkFulfilledEntry(result.z, "z-value");
    checkRejectedEntry(result.a, "a-reason");
    checkFulfilledEntry(result[sym], "sym-value");
}

// Non-object argument rejects asynchronously with a TypeError.
{
    let reason;
    Promise.allSettledKeyed(123).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(reason instanceof TypeError, true);
}

// Entry objects from different calls are distinct but same-shaped.
{
    let first;
    let second;
    Promise.allSettledKeyed({ k: 1 }).then((value) => { first = value; });
    Promise.allSettledKeyed({ k: Promise.reject(2) }).then((value) => { second = value; });
    drainMicrotasks();
    checkFulfilledEntry(first.k, 1);
    checkRejectedEntry(second.k, 2);
    shouldBe(first.k !== second.k, true);
}
