// Regression test: the async generator driver creates its iterator-result object in the GENERATOR's
// realm (matching the spec, V8, and SpiderMonkey), and the thenable-check that governs the driver
// settle uses that same (generator's) realm's promise-then watchpoint -- not the consumer's realm.
// This also guards the per-generator iterator-result cache from leaking an object of the wrong realm.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function isIterResult(o) {
    return o && typeof o === "object"
        && Object.prototype.hasOwnProperty.call(o, "value")
        && Object.prototype.hasOwnProperty.call(o, "done");
}

let error = null;

// 1. Cross-realm for-await delivers correct values, and the results belong to the generator's realm.
(function () {
    let B = createGlobalObject();
    B.eval("globalThis.mk = async function*(){ yield 1; yield 2; yield 3; };");

    // A `then` getter in the GENERATOR realm captures the result objects (proving their realm) and,
    // because it fires the generator realm's watchpoint, the settle must take the resolve path there.
    B.eval(`
        globalThis.captured = [];
        Object.defineProperty(Object.prototype, "then", {
            configurable: true,
            get() {
                if (this && typeof this === "object"
                    && Object.prototype.hasOwnProperty.call(this, "value")
                    && Object.prototype.hasOwnProperty.call(this, "done"))
                    globalThis.captured.push(this);
                return undefined;
            },
        });
    `);

    let delivered = [];
    (async () => {
        for await (const x of B.mk())
            delivered.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBe(delivered.join(","), "1,2,3");
    // The generator-realm then getter must have observed the results (watchpoint = generator realm).
    shouldBe(B.captured.length > 0, true);
    // Every captured object is a generator-realm object.
    for (const r of B.captured)
        shouldBe(Object.getPrototypeOf(r), B.Object.prototype);
    // The distinct value-carrying results must be 1/2/3 (a cross-yield alias would collapse these).
    let distinct = [...new Set(B.captured)];
    let values = distinct.filter(r => r.done === false).map(r => r.value).sort((a, b) => a - b);
    shouldBe(values.join(","), "1,2,3");
})();

// 2. A `then` getter in the CONSUMER realm must NOT be consulted for generator-realm results: those
// results are governed by the generator realm's (valid) watchpoint, so the fast path is taken and the
// consumer realm's then is irrelevant.
(function () {
    let B = createGlobalObject();
    B.eval("globalThis.mk = async function*(){ yield 10; yield 20; };");

    let consumerReads = 0;
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            if (isIterResult(this))
                ++consumerReads;
            return undefined;
        },
    });

    let delivered = [];
    (async () => {
        for await (const x of B.mk())
            delivered.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    delete Object.prototype.then;
    if (error)
        throw error;

    shouldBe(delivered.join(","), "10,20");
    shouldBe(consumerReads, 0);
})();

// 3. Cross-realm reuse correctness: driving a generator from another realm across many yields must
// deliver every value correctly (the reused cached object stays a valid generator-realm object).
(function () {
    let B = createGlobalObject();
    B.eval("globalThis.mk = async function*(){ for (let i = 0; i < 300; ++i) yield i; };");

    let collected = [];
    (async () => {
        for await (const x of B.mk())
            collected.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBe(collected.length, 300);
    for (let i = 0; i < 300; ++i)
        shouldBe(collected[i], i);
})();

// 4. Adversarial: a realm-B generator whose @@asyncIterator/next are replaced by THIS realm's
// primordials (so the consumer's cooperative driver drives a foreign-realm generator). The iterator
// result must still be created in the GENERATOR's realm (matches V8/SpiderMonkey and manual .next()),
// consistently across every yield -- never the consumer's realm.
(function () {
    let A_agp = Object.getPrototypeOf(Object.getPrototypeOf((async function* () { })()));
    let B = createGlobalObject();
    B.eval("globalThis.mk = async function*(){ yield 1; yield 2; yield 3; };");
    let g = B.mk();
    Object.defineProperty(g, Symbol.asyncIterator, { value: A_agp[Symbol.asyncIterator], writable: true, configurable: true });
    Object.defineProperty(g, "next", { value: A_agp.next, writable: true, configurable: true });

    let consumerReads = 0;
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            if (isIterResult(this))
                ++consumerReads;
            return undefined;
        },
    });
    B.eval(`
        globalThis.generatorReads = 0;
        Object.defineProperty(Object.prototype, "then", {
            configurable: true,
            get() {
                if (this && typeof this === "object"
                    && Object.prototype.hasOwnProperty.call(this, "value")
                    && Object.prototype.hasOwnProperty.call(this, "done"))
                    ++globalThis.generatorReads;
                return undefined;
            },
        });
    `);

    let delivered = [];
    (async () => {
        for await (const x of g)
            delivered.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    delete Object.prototype.then;
    if (error)
        throw error;

    shouldBe(delivered.join(","), "1,2,3");
    // Results are generator-realm objects: the consumer realm's then must never be consulted for them.
    shouldBe(consumerReads, 0);
    shouldBe(B.generatorReads > 0, true);
})();

// 5. Manual .next() from this realm on a foreign-realm generator: every result object (a yield, the
// terminal done, and the already-completed fast path) belongs to the GENERATOR's realm, matching V8
// and SpiderMonkey. The returned promise stays this (caller) realm.
(function () {
    let A_next = Object.getPrototypeOf(Object.getPrototypeOf((async function* () { })())).next;
    let B = createGlobalObject();
    B.eval("globalThis.mk = async function*(){ yield 1; };");
    let g = B.mk();

    let results = [];
    let promiseRealmIsCaller = null;
    (async () => {
        let p = A_next.call(g);
        promiseRealmIsCaller = Object.getPrototypeOf(p) === Promise.prototype;
        results.push(await p);            // yield 1
        results.push(await A_next.call(g)); // done
        results.push(await A_next.call(g)); // already-completed fast path
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBe(results[0].value, 1);
    shouldBe(results[1].done, true);
    shouldBe(results[2].done, true);
    // All three iterator results are in the generator's realm.
    for (const r of results)
        shouldBe(Object.getPrototypeOf(r), B.Object.prototype);
    // The promise from next() is the caller's realm.
    shouldBe(promiseRealmIsCaller, true);
})();
