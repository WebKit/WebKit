// The isDefinitelyNonThenable Structure cache compares structure->realm() against
// the calling JSGlobalObject before trusting a cached `true`. This test exercises
// cross-realm Promise resolution to ensure a cache populated by one realm's
// watchpoint set is never trusted by another realm.

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

const other = createGlobalObject();
const otherObject = other.Function("return { a: 1, b: 2, c: 3 };")();
const otherProto = other.Object.prototype;

// --- 1. Resolve a foreign object literal in this realm. ---
asyncTest(async function resolveForeignObject() {
    for (let i = 0; i < testLoopCount; i++) {
        const o = await Promise.resolve(otherObject);
        shouldBe(o.a, 1);
        shouldBe(o.b, 2);
        shouldBe(o.c, 3);
    }
});

// --- 2. The other realm warms its own cache, then this realm resolves the
//        same Structure. Each realm's watchpoint set must guard independently. ---
asyncTest(async function bothRealmsWarm() {
    // Warm the other realm.
    other.Function("p", `
        for (let i = 0; i < 100; i++)
            p.resolve({ a: i });
    `)(other.Promise);
    other.drainMicrotasks();

    // Now resolve foreign objects from this realm.
    const factory = other.Function("i", "return { a: i, b: i * 2 };");
    for (let i = 0; i < 100; i++) {
        const o = await Promise.resolve(factory(i));
        shouldBe(o.a, i, "bothRealmsWarm.a");
        shouldBe(o.b, i * 2, "bothRealmsWarm.b");
    }
});

// --- 3. Poison the OTHER realm's Object.prototype.then. The structure of
//        otherObject lives in the other realm; resolving it from THIS realm
//        must observe the foreign `then`. ---
asyncTest(async function poisonOtherRealmObjectProto() {
    // Warm-up resolves from this realm.
    const factory = other.Function("i", "return { p: i };");
    for (let i = 0; i < 100; i++) {
        const o = await Promise.resolve(factory(i));
        shouldBe(o.p, i, "poisonOtherRealm.warm");
    }
    // Poison the foreign Object.prototype.
    other.Function(`
        Object.prototype.then = function(resolve) { resolve("foreign-poisoned"); };
    `)();
    try {
        const v = await Promise.resolve(factory(42));
        shouldBe(v, "foreign-poisoned", "poisonOtherRealm.afterPoison");
    } finally {
        other.Function("delete Object.prototype.then;")();
    }
});
