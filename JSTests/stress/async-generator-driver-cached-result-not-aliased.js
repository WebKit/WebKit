// Regression test: the async generator driver reuses a per-generator cached iterator result
// object on the internal for-await driver path (an optimization that avoids allocating a fresh
// { value, done } object per yield). That reuse is only safe while the object cannot be observed
// by user code. When Object.prototype.then is defined (firing the promise-then watchpoint), the
// settle path reads `.then` on the iterator result, so a `then` getter runs with the iterator
// result as its receiver and can capture it. In that case the driver must NOT hand out the reused
// cached object; each settled result must be a distinct, unmutated object (as if freshly created).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

let error = null;

// 1. Reuse fast path (no observable then): delivered values must be correct across many yields.
(function () {
    async function* gen() {
        for (let i = 0; i < 500; ++i)
            yield { i };
    }

    let collected = [];
    (async function () {
        for await (const x of gen())
            collected.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;
    shouldBe(collected.length, 500);
    for (let i = 0; i < 500; ++i)
        shouldBe(collected[i].i, i);
})();

// 2. Observable `then` getter: the iterator result escapes to user code, so results must be
// distinct, unmutated objects. A reused/aliased cached object would show up as a single identity
// whose value/done were overwritten by later yields.
(function () {
    let captured = [];
    Object.defineProperty(Object.prototype, "then", {
        configurable: true,
        get() {
            if (this && typeof this === "object"
                && Object.prototype.hasOwnProperty.call(this, "value")
                && Object.prototype.hasOwnProperty.call(this, "done")) {
                captured.push({ result: this, value: this.value, done: this.done });
            }
            return undefined;
        },
    });

    async function* gen() {
        yield 10;
        yield 20;
        yield 30;
    }

    let delivered = [];
    (async function () {
        for await (const x of gen())
            delivered.push(x);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    delete Object.prototype.then;

    if (error)
        throw error;

    // for-await must still deliver the correct values.
    shouldBe(delivered.join(","), "10,20,30");

    // Every captured iterator result must be a distinct object.
    shouldBe(new Set(captured.map(c => c.result)).size, captured.length);

    // The value observed at capture time must not have been mutated afterwards by reuse.
    for (const c of captured)
        shouldBe(c.result.value, c.value);

    // The three value-carrying results must retain 10/20/30.
    let values = captured.filter(c => c.done === false).map(c => c.value);
    shouldBe(values.join(","), "10,20,30");
})();

// 3. Watchpoint invalidated mid-lifetime: a generator driven first under the fast path (cached
// object created) and later under an observable then must not leak the cached object.
(function () {
    let captured = [];

    async function* gen() {
        yield "a";
        yield "b";
        yield "c";
        yield "d";
    }

    let delivered = [];
    (async function () {
        let n = 0;
        for await (const x of gen()) {
            delivered.push(x);
            if (++n === 2) {
                Object.defineProperty(Object.prototype, "then", {
                    configurable: true,
                    get() {
                        if (this && typeof this === "object"
                            && Object.prototype.hasOwnProperty.call(this, "value")
                            && Object.prototype.hasOwnProperty.call(this, "done"))
                            captured.push(this);
                        return undefined;
                    },
                });
            }
        }
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    delete Object.prototype.then;

    if (error)
        throw error;
    shouldBe(delivered.join(","), "a,b,c,d");
    shouldBe(new Set(captured).size, captured.length);
})();
