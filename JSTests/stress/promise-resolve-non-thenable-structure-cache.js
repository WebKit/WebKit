// Stress test for the Structure-level isDefinitelyNonThenable cache.
//
// The cache stores "this Structure's prototype chain definitely cannot resolve a
// thenable" in a 2-bit field on Structure. A `true` cache is sound only while the
// realm's promiseThenWatchpointSet (which also watches `then` absence on
// Object.prototype) is intact. Each test below stresses a path where the cache
// might miscompile if the watchpoint or fallback walk were wrong.

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

// --- 1. Plain object literal: cache hit fast path ---
asyncTest(async function plainObjectLiteral() {
    for (let i = 0; i < testLoopCount; i++) {
        const o = await { a: i, b: i + 1, c: i + 2 };
        shouldBe(o.a, i);
        shouldBe(o.b, i + 1);
        shouldBe(o.c, i + 2);
    }
});

// --- 2. Promise.resolve with object literal ---
asyncTest(async function promiseResolveObjectLiteral() {
    for (let i = 0; i < testLoopCount; i++) {
        const o = await Promise.resolve({ x: i, y: i * 2 });
        shouldBe(o.x, i);
        shouldBe(o.y, i * 2);
    }
});

// --- 3. Object with own `then` property (data property) ---
asyncTest(async function ownThenDataProperty() {
    for (let i = 0; i < 100; i++) {
        const v = await { then(resolve) { resolve(i); } };
        shouldBe(v, i, "ownThenDataProperty");
    }
});

// --- 4. Object with own `then` getter ---
asyncTest(async function ownThenGetter() {
    for (let i = 0; i < 100; i++) {
        const v = await { get then() { return (resolve) => resolve(i * 3); } };
        shouldBe(v, i * 3, "ownThenGetter");
    }
});

// --- 5. Null prototype object ---
asyncTest(async function nullProto() {
    for (let i = 0; i < 100; i++) {
        const o = Object.create(null);
        o.a = i;
        const v = await o;
        shouldBe(v.a, i, "nullProto");
    }
});

// --- 6. Frozen / sealed objects ---
asyncTest(async function frozenSealed() {
    const f = Object.freeze({ a: 1 });
    shouldBe((await f).a, 1, "frozen");
    const s = Object.seal({ b: 2 });
    shouldBe((await s).b, 2, "sealed");
    const p = Object.preventExtensions({ c: 3 });
    shouldBe((await p).c, 3, "preventExtensions");
});

// --- 7. Class instance: deep prototype chain (Uncacheable state) ---
asyncTest(async function classInstanceDeepChain() {
    class A { constructor() { this.a = 1; } }
    class B extends A { constructor() { super(); this.b = 2; } }
    class C extends B { constructor() { super(); this.c = 3; } }
    for (let i = 0; i < testLoopCount; i++) {
        const o = await new C();
        shouldBe(o.a, 1);
        shouldBe(o.b, 2);
        shouldBe(o.c, 3);
    }
});

// --- 8. Class proto gains `then` after cache warm-up ---
asyncTest(async function classProtoGainsThen() {
    class Foo { constructor() { this.x = 1; } }
    // Warm.
    for (let i = 0; i < 100; i++) {
        const o = await new Foo();
        shouldBe(o.x, 1);
    }
    // Poison the proto. The Structure for Foo instances is Uncacheable
    // (chain depth >= 2), so this MUST be picked up immediately by the walk.
    Foo.prototype.then = function(resolve) { resolve("foo-thenable"); };
    const v = await new Foo();
    shouldBe(v, "foo-thenable", "classProtoGainsThen");
    delete Foo.prototype.then;
    // After delete, behavior should revert.
    const o2 = await new Foo();
    shouldBe(o2.x, 1, "classProtoThenDeleted");
});

// --- 9. Many distinct shapes: Structure-level cache must not bleed across shapes ---
asyncTest(async function megamorphicShapes() {
    function makeShape(i) {
        const o = {};
        for (let j = 0; j <= i % 8; j++)
            o["p" + j] = j;
        return o;
    }
    for (let i = 0; i < 200; i++) {
        const o = await makeShape(i);
        shouldBe(o.p0, 0);
    }
});

// --- 10. Property addition transitions to a fresh Structure with NotComputed state ---
asyncTest(async function structureTransition() {
    for (let i = 0; i < 100; i++) {
        const o = { a: 1 };
        shouldBe((await o).a, 1);
        o.b = 2; // transition to a new Structure
        shouldBe((await o).b, 2);
        o.then = function(resolve) { resolve("tail-then"); }; // another transition; now thenable
        const v = await o;
        shouldBe(v, "tail-then", "structureTransition.then");
    }
});

// --- 11. for-await-of (iterator result objects {value, done}) ---
asyncTest(async function forAwaitOf() {
    async function* gen() {
        yield { v: 1 };
        yield { v: 2 };
        yield { v: 3 };
    }
    let sum = 0;
    for (let i = 0; i < 100; i++) {
        for await (const x of gen())
            sum += x.v;
    }
    shouldBe(sum, 600, "forAwaitOf");
});

// --- 12. async generator returning object literals ---
asyncTest(async function asyncGenObjectLiterals() {
    async function* g() {
        for (let i = 0; i < 50; i++)
            yield { i };
    }
    let acc = 0;
    for await (const o of g())
        acc += o.i;
    shouldBe(acc, 1225, "asyncGenObjectLiterals");
});

// --- 13. Promise.all with mixed thenables and non-thenables ---
asyncTest(async function promiseAllMixed() {
    const results = await Promise.all([
        { a: 1 },
        Promise.resolve({ a: 2 }),
        { then(resolve) { resolve({ a: 3 }); } },
        Object.create(null, { a: { value: 4 } }),
    ]);
    shouldBe(results[0].a, 1);
    shouldBe(results[1].a, 2);
    shouldBe(results[2].a, 3);
    shouldBe(results[3].a, 4);
});

// --- 14. Same Structure shared across many objects ---
asyncTest(async function sharedStructure() {
    function make(i) { return { x: i, y: i, z: i }; }
    for (let i = 0; i < testLoopCount; i++) {
        const o = await make(i);
        shouldBe(o.x, i);
    }
});

// --- 15. Symbol-keyed properties don't poison the cache ---
asyncTest(async function symbolKeysHarmless() {
    const sym = Symbol("k");
    for (let i = 0; i < 100; i++) {
        const o = { a: i, [sym]: i * 10 };
        const v = await o;
        shouldBe(v.a, i);
        shouldBe(v[sym], i * 10);
    }
});

// --- 16. then defined as accessor on the object's prototype ---
asyncTest(async function thenAccessorOnProto() {
    function Wrapper(v) { this.v = v; }
    Object.defineProperty(Wrapper.prototype, "then", {
        get() { const v = this.v; return (resolve) => resolve("w" + v); },
        configurable: true,
    });
    const r = await new Wrapper(7);
    shouldBe(r, "w7", "thenAccessorOnProto");
});

// --- 17. `then` added in the middle of a deep prototype chain ---
asyncTest(async function thenMidChain() {
    class A { constructor() { this.a = 1; } }
    class B extends A { constructor() { super(); this.b = 2; } }
    class C extends B { constructor() { super(); this.c = 3; } }
    // Warm.
    for (let i = 0; i < 100; i++)
        shouldBe((await new C()).c, 3);
    // Add `then` to A.prototype: two hops away from a C instance, but still on the chain.
    A.prototype.then = function(resolve) { resolve("mid-chain"); };
    try {
        const v = await new C();
        shouldBe(v, "mid-chain", "thenMidChain");
    } finally {
        delete A.prototype.then;
    }
    shouldBe((await new C()).c, 3, "thenMidChain.afterDelete");
});

// --- 18. Object.setPrototypeOf changes the chain after warm-up ---
asyncTest(async function setPrototypeOf() {
    function MakeProto() {}
    const obj = { v: 1 };
    Object.setPrototypeOf(obj, MakeProto.prototype); // transition to a new Structure
    for (let i = 0; i < 100; i++)
        shouldBe((await obj).v, 1);
    // Replace the proto with a thenable. This causes another transition.
    Object.setPrototypeOf(obj, { then(resolve) { resolve("sp-thenable"); } });
    const v = await obj;
    shouldBe(v, "sp-thenable", "setPrototypeOf");
});

// --- 19. Reflect.construct with custom newTarget (poly-proto-ish path) ---
asyncTest(async function reflectConstruct() {
    function Base() { this.b = 1; }
    function Derived() { this.d = 2; }
    Derived.prototype = Object.create(Base.prototype);
    for (let i = 0; i < 100; i++) {
        const o = Reflect.construct(Base, [], Derived);
        const v = await o;
        shouldBe(v.b, 1, "reflectConstruct");
    }
});

// --- 20. Array and TypedArray (built-in prototype chains) ---
asyncTest(async function builtins() {
    const arr = await [1, 2, 3];
    shouldBe(arr.length, 3, "array");
    const ta = await new Uint8Array([4, 5, 6]);
    shouldBe(ta[0], 4, "typedArray");
    const map = await new Map([[1, "a"]]);
    shouldBe(map.get(1), "a", "map");
});

// --- 21. Proxy: getOwnPropertyDescriptor trap must always be consulted ---
asyncTest(async function proxyAlwaysSlow() {
    let trapCalled = false;
    const target = { a: 1 };
    const handler = {
        get(t, k) {
            if (k === "then")
                trapCalled = true;
            return Reflect.get(t, k);
        },
    };
    const v = await new Proxy(target, handler);
    shouldBe(v.a, 1, "proxy.value");
    shouldBe(trapCalled, true, "proxy.trapCalled");
});

// --- 22. Object.prototype gains a NON-`then` property after warm-up.
//          The adaptive watchpoint must re-arm on the new Structure (not fire),
//          so the cache stays usable AND the result stays correct. ---
asyncTest(async function objectProtoNonThenAdditionHarmless() {
    // Warm.
    for (let i = 0; i < testLoopCount; i++)
        shouldBe((await { a: i, b: i }).a, i);
    Object.prototype.someUnrelatedProperty = 42;
    try {
        for (let i = 0; i < 100; i++) {
            const o = await { a: i, b: i };
            shouldBe(o.a, i, "nonThenAddition.a");
            shouldBe(o.someUnrelatedProperty, 42, "nonThenAddition.proto");
        }
    } finally {
        delete Object.prototype.someUnrelatedProperty;
    }
});

// --- 23. Object.prototype.then poison MUST come last:
//          it permanently fires the watchpoint set for this realm. ---
asyncTest(async function objectProtoThenPoisonLast() {
    // Warm the cache for several Structures.
    for (let i = 0; i < testLoopCount; i++)
        await { a: i, b: i, c: i };

    Object.prototype.then = function(resolve) { resolve("op-poisoned"); };
    try {
        const v1 = await { a: 1, b: 2, c: 3 };
        shouldBe(v1, "op-poisoned", "objectProtoThenPoison.literal");
        const v2 = await { p0: 0 }; // a different shape, also covered
        shouldBe(v2, "op-poisoned", "objectProtoThenPoison.otherShape");
        class Z { constructor() { this.z = 1; } }
        const v3 = await new Z(); // class instance: walk reaches Object.prototype
        shouldBe(v3, "op-poisoned", "objectProtoThenPoison.classInstance");
    } finally {
        delete Object.prototype.then;
    }
    // After delete, watchpoint stays fired -> always slow path, but result is correct.
    const v4 = await { a: 1, b: 2, c: 3 };
    shouldBe(v4.a, 1, "afterDelete.a");
    shouldBe(v4.b, 2, "afterDelete.b");
});
