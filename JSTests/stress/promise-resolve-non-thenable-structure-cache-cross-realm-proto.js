// The isDefinitelyNonThenable Structure cache may only cache NonThenable when the
// prototype is the Object.prototype of the *structure's* realm: a cached `true` is
// guarded by structure->realm()'s promiseThenWatchpointSet, which only watches that
// realm's Object.prototype. This test mixes a structure from one realm with the
// Object.prototype of another realm and verifies that adding `then` to the foreign
// prototype is always observed.

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

// --- 1. Structure from this realm, prototype from the other realm. The other
//        realm warms the cache; poisoning the other realm's Object.prototype
//        must still be observed when resolving from this realm. ---
asyncTest(async function localStructureForeignProto() {
    const otherProto = other.Object.prototype;
    function make() {
        const o = { a: 1 };
        Object.setPrototypeOf(o, otherProto);
        return o;
    }

    // Warm up from the other realm: must not cache NonThenable on this realm's
    // structure, since this realm's watchpoint set does not watch otherProto.
    for (let i = 0; i < 100; i++)
        other.Promise.resolve(make());
    other.drainMicrotasks();

    // Fires only the other realm's watchpoint.
    other.Function(`
        Object.prototype.then = function(resolve) { resolve("foreign-proto-poisoned"); };
    `)();
    try {
        const v = await Promise.resolve(make());
        shouldBe(v, "foreign-proto-poisoned", "localStructureForeignProto");
    } finally {
        other.Function("delete Object.prototype.then;")();
    }
});

// --- 2. Mirror: structure from the other realm, prototype from this realm.
//        This realm warms the cache; poisoning this realm's Object.prototype
//        must still be observed when resolving from the other realm. ---
asyncTest(async function foreignStructureLocalProto() {
    const make = other.Function("proto", `
        const o = { b: 2 };
        Object.setPrototypeOf(o, proto);
        return o;
    `);

    for (let i = 0; i < 100; i++)
        Promise.resolve(make(Object.prototype));
    drainMicrotasks();

    const resolveInOther = other.Function("value", "onSettled", `
        Promise.resolve(value).then(onSettled);
    `);

    // Fires only this realm's watchpoint.
    Object.prototype.then = function(resolve) { resolve("local-proto-poisoned"); };
    try {
        let settled;
        resolveInOther(make(Object.prototype), (v) => { settled = v; });
        other.drainMicrotasks();
        drainMicrotasks();
        shouldBe(settled, "local-proto-poisoned", "foreignStructureLocalProto");
    } finally {
        delete Object.prototype.then;
    }
});
