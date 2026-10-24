//@ requireOptions("--usePromiseAllKeyed=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function shouldBeArray(actual, expected) {
    shouldBe(JSON.stringify(actual), JSON.stringify(expected));
}

// Thenables: |then| is invoked once per key with resolve-element functions, and an ill-behaved
// thenable that calls back multiple times cannot corrupt the count.
{
    let thenCalls = [];
    let onFulfilledFunctions = [];
    let input = {
        a: {
            then(onFulfilled, onRejected) {
                thenCalls.push("a");
                onFulfilledFunctions.push(onFulfilled);
                onFulfilled("first-a");
                onFulfilled("second-a"); // Ignored: already called.
                onRejected("rejected-a"); // Ignored: already called.
            }
        },
        b: {
            then(onFulfilled, onRejected) {
                thenCalls.push("b");
                onFulfilledFunctions.push(onFulfilled);
                onFulfilled("first-b");
            }
        },
    };

    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(thenCalls, ["a", "b"]);
    shouldBe(result.a, "first-a");
    shouldBe(result.b, "first-b");

    // The resolve-element functions are anonymous, extensible, non-constructor functions with length 1.
    let resolveElement = onFulfilledFunctions[0];
    shouldBe(typeof resolveElement, "function");
    shouldBe(resolveElement.length, 1);
    shouldBe(resolveElement.name, "");
    shouldBe(Object.getPrototypeOf(resolveElement), Function.prototype);
    shouldBe(Object.prototype.hasOwnProperty.call(resolveElement, "prototype"), false);
    shouldBe(Object.isExtensible(resolveElement), true);
    let constructed = true;
    try {
        Reflect.construct(function () { }, [], resolveElement);
    } catch {
        constructed = false;
    }
    shouldBe(constructed, false);
    shouldBe(resolveElement !== onFulfilledFunctions[1], true);
}

// A thenable that synchronously resolves all elements inside |then| resolves the combined
// promise only once, after the loop finishes (remainingElementsCount starts at 1).
{
    let resolutions = [];
    let input = {
        a: { then(onFulfilled) { onFulfilled("a"); } },
        b: { then(onFulfilled) { onFulfilled("b"); } },
    };
    Promise.allKeyed(input).then((value) => { resolutions.push(value); });
    drainMicrotasks();
    shouldBe(resolutions.length, 1);
    shouldBe(resolutions[0].a, "a");
    shouldBe(resolutions[0].b, "b");
}

// Synchronous exception while getting a value rejects the combined promise.
{
    let reason;
    let input = { a: Promise.resolve(1) };
    Object.defineProperty(input, "b", { get() { throw new Error("get b"); }, enumerable: true });
    Promise.allKeyed(input).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(String(reason), "Error: get b");
}

// [[OwnPropertyKeys]] is snapshotted first, then per-key [[GetOwnProperty]] / Get run just before
// the key is processed: a getter that deletes a later property hides it, and one added during
// iteration is not visited.
{
    let input = {
        get a() {
            delete input.b;
            input.added = Promise.resolve("added");
            return Promise.resolve("a");
        },
        b: Promise.resolve("b"),
        c: Promise.resolve("c"),
    };
    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(Reflect.ownKeys(result), ["a", "c"]);
    shouldBe(result.a, "a");
    shouldBe(result.c, "c");
}

// A non-enumerable property that becomes enumerable after [[OwnPropertyKeys]] is included, since
// enumerability is checked per key.
{
    let input = { a: Promise.resolve("a") };
    Object.defineProperty(input, "b", { value: Promise.resolve("b"), enumerable: false, configurable: true });
    Object.defineProperty(input, "a", {
        enumerable: true,
        get() {
            Object.defineProperty(input, "b", { enumerable: true });
            return Promise.resolve("a");
        },
    });
    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(Reflect.ownKeys(result), ["a", "b"]);
    shouldBe(result.b, "b");
}

// Proxy: internal method invocation order matches the specification
// ([[OwnPropertyKeys]] once, then per key: [[GetOwnProperty]] and Get; no Get for non-enumerable).
{
    let log = [];
    let target = { a: Promise.resolve("a"), b: Promise.resolve("b") };
    Object.defineProperty(target, "hidden", { value: Promise.resolve("hidden"), enumerable: false });
    let proxy = new Proxy(target, {
        ownKeys(t) { log.push("ownKeys"); return Reflect.ownKeys(t); },
        getOwnPropertyDescriptor(t, key) { log.push(`gopd:${String(key)}`); return Reflect.getOwnPropertyDescriptor(t, key); },
        get(t, key, receiver) { log.push(`get:${String(key)}`); return Reflect.get(t, key, receiver); },
    });
    let result;
    Promise.allKeyed(proxy).then((value) => { result = value; });
    drainMicrotasks();
    shouldBeArray(log, ["ownKeys", "gopd:a", "get:a", "gopd:b", "get:b", "gopd:hidden"]);
    shouldBeArray(Reflect.ownKeys(result), ["a", "b"]);
    shouldBe(result.a, "a");
    shouldBe(result.b, "b");
}

// [[OwnPropertyKeys]] throwing rejects the promise.
{
    let reason;
    let proxy = new Proxy({}, {
        ownKeys() { throw new Error("ownKeys throws"); },
    });
    Promise.allKeyed(proxy).catch((error) => { reason = error; });
    drainMicrotasks();
    shouldBe(String(reason), "Error: ownKeys throws");
}

// Index keys mixed with named keys: index-keyed elements live in indexed storage and named ones at
// property offsets; both are filled correctly regardless of settlement order, and the result's key
// order follows ordinary [[OwnPropertyKeys]] (indices ascending first).
{
    let resolvers = { };
    let input = { };
    for (let key of ["b", 3, "a", 0, 4294967294, "4294967295", 1])
        input[key] = new Promise((resolve) => { resolvers[key] = resolve; });
    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    for (let key of ["4294967295", 1, "a", 4294967294, 0, "b", 3])
        resolvers[key]("v" + String(key));
    drainMicrotasks();
    let keys = Reflect.ownKeys(result);
    shouldBeArray(keys, ["0", "1", "3", "4294967294", "b", "a", "4294967295"]);
    for (let key of keys)
        shouldBe(result[key], "v" + key);
}

// "__proto__" as an own enumerable key becomes an own data property of the null-prototype result.
{
    let input = JSON.parse('{"__proto__": 1, "x": 2}');
    input.__proto__ = Promise.resolve("proto-value");
    let result;
    Promise.allKeyed(input).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(Object.getPrototypeOf(result), null);
    shouldBeArray(Reflect.ownKeys(result), ["__proto__", "x"]);
    shouldBe(Object.getOwnPropertyDescriptor(result, "__proto__").value, "proto-value");
    shouldBe(result.x, 2);
}

// More keys than the inline capacity / a dictionary-sized key set: values land in the right slots
// even when settled in reverse order, and property order is preserved.
{
    for (let count of [7, 70, 300]) {
        let input = { };
        let resolvers = [];
        for (let i = 0; i < count; ++i)
            input["k" + i] = (i % 3 === 0) ? i : new Promise((resolve) => { resolvers.push([i, resolve]); });
        let result;
        Promise.allKeyed(input).then((value) => { result = value; });
        for (let j = resolvers.length - 1; j >= 0; --j)
            resolvers[j][1](resolvers[j][0]);
        drainMicrotasks();
        let keys = Reflect.ownKeys(result);
        shouldBe(keys.length, count);
        for (let i = 0; i < count; ++i) {
            shouldBe(keys[i], "k" + i);
            shouldBe(result["k" + i], i);
        }
    }
}

// Results for the same key set share one Structure across calls.
{
    let first;
    let second;
    Promise.allKeyed({ shared: 1, other: Promise.resolve("x") }).then((value) => { first = value; });
    Promise.allKeyed({ shared: 2, other: "y" }).then((value) => { second = value; });
    drainMicrotasks();
    shouldBe(first.other, "x");
    shouldBe(second.other, "y");
    let structureOf = (object) => describe(object).match(/Structure (0x[0-9a-f]+|[0-9]+)/)[1];
    shouldBe(structureOf(first), structureOf(second));
}

// A promise whose `then` is an own getter goes through the observable then path.
{
    let promise = Promise.resolve("value");
    let thenGets = 0;
    Object.defineProperty(promise, "then", {
        get() {
            thenGets++;
            return Promise.prototype.then;
        },
    });
    let result;
    Promise.allKeyed({ p: promise }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.p, "value");
    shouldBe(thenGets >= 1, true);
}

// A value whose `then` is not callable but not undefined: treated as non-thenable, fulfills.
{
    let result;
    Promise.allKeyed({ a: { then: 42 } }).then((value) => { result = value; });
    drainMicrotasks();
    shouldBe(result.a.then, 42);
}
