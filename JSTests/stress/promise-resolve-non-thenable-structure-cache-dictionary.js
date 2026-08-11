//@ requireOptions("--useDollarVM=1")
// Dictionary structures are mutated in place when properties are added, so the
// Structure-level isDefinitelyNonThenable cache must not store NonThenable for
// them: a later in-place addition of `then` would not invalidate the cache and
// the promise would resolve with the object instead of calling its `then`.
//
// These tests warm the cache on a dictionary structure, then add `then` in
// place and check that promise resolution observes it.

function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error(`bad value (${msg ?? ""}): got ${String(actual)}, expected ${String(expected)}`);
}

function asyncTest(fn) {
    let done = false;
    let error;
    fn().then(() => { done = true; }, (e) => { error = e; done = true; });
    drainMicrotasks();
    if (!done)
        throw new Error("async test did not settle: " + fn.name);
    if (error)
        throw error;
}

// --- 1. CacheableDictionary: add `then` in place after warm-up ---
asyncTest(async function cacheableDictionaryGainsThen() {
    const o = { a: 1, b: 2, c: 3 };
    $vm.toCacheableDictionary(o);

    // Warm: would tempt the cache to record NonThenable on the dictionary structure.
    for (let i = 0; i < testLoopCount; i++)
        shouldBe((await o).a, 1, "cacheableDict.warm");

    // In-place mutation: same Structure object, now thenable.
    o.then = function (resolve) { resolve("dict-then"); };
    for (let i = 0; i < 100; i++)
        shouldBe(await o, "dict-then", "cacheableDict.poisoned");

    // In-place removal: must also be observed (only loses the optimization if cached as MaybeThenable).
    delete o.then;
    for (let i = 0; i < 100; i++)
        shouldBe((await o).a, 1, "cacheableDict.afterDelete");
});

// --- 2. UncacheableDictionary: same, after delete forces UncacheableDictionary ---
asyncTest(async function uncacheableDictionaryGainsThen() {
    const o = { a: 1, b: 2, c: 3 };
    $vm.toUncacheableDictionary(o);

    for (let i = 0; i < testLoopCount; i++)
        shouldBe((await o).a, 1, "uncacheableDict.warm");

    o.then = function (resolve) { resolve("uncacheable-dict-then"); };
    for (let i = 0; i < 100; i++)
        shouldBe(await o, "uncacheable-dict-then", "uncacheableDict.poisoned");

    delete o.then;
    for (let i = 0; i < 100; i++)
        shouldBe((await o).a, 1, "uncacheableDict.afterDelete");
});

// --- 3. CacheableDictionary via Promise.resolve ---
asyncTest(async function cacheableDictionaryPromiseResolve() {
    const o = { x: 42 };
    $vm.toCacheableDictionary(o);

    for (let i = 0; i < testLoopCount; i++)
        shouldBe((await Promise.resolve(o)).x, 42, "cacheableDict.resolve.warm");

    o.then = function (resolve) { resolve("resolved-then"); };
    for (let i = 0; i < 100; i++)
        shouldBe(await Promise.resolve(o), "resolved-then", "cacheableDict.resolve.poisoned");
});

// --- 4. Dictionary with null prototype: shortest cacheable chain shape ---
asyncTest(async function dictionaryNullProtoGainsThen() {
    const o = Object.create(null);
    o.a = 1;
    $vm.toCacheableDictionary(o);

    for (let i = 0; i < testLoopCount; i++)
        shouldBe((await o).a, 1, "nullProtoDict.warm");

    o.then = function (resolve) { resolve("null-proto-then"); };
    for (let i = 0; i < 100; i++)
        shouldBe(await o, "null-proto-then", "nullProtoDict.poisoned");
});
