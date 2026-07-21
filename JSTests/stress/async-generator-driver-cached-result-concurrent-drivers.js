// Regression test: the async generator driver reuses a per-generator cached iterator result object
// on the internal for-await driver path. That reuse is only safe when the previously delivered
// object has already been consumed. A single driver consumes serially, but one async generator can
// be driven by several consumers at once (e.g. two `for await` loops over the same generator). When
// the generator settles one driver and then, in the same synchronous turn, settles another before
// the first driver's resume microtask runs, mutating a single shared cached object would corrupt the
// value already handed to the first driver. Each in-flight driver must therefore see its own result.

function shouldBe(actual, expected) {
    if (String(actual) !== String(expected))
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

let error = null;

// 1. Two `for await` loops over one generator that yields a single value: the first loop must get
// the value, the second must get nothing (the generator is already draining for the first).
(function () {
    async function* gen() { yield 1; }
    let g = gen();
    let a = [], b = [];
    (async () => { for await (const x of g) a.push(x); })().then(() => { }, e => { error = e; });
    (async () => { for await (const x of g) b.push(x); })().then(() => { }, e => { error = e; });
    drainMicrotasks();
    if (error)
        throw error;
    shouldBe(a, [1]);
    shouldBe(b, []);
})();

// 2. Multiple yields split across two concurrent loops: every value is delivered exactly once, to
// exactly one loop, with none lost or aliased across the two in-flight cached deliveries.
(function () {
    async function* gen() { for (let i = 0; i < 100; ++i) yield i; }
    let g = gen();
    let a = [], b = [];
    (async () => { for await (const x of g) a.push(x); })().then(() => { }, e => { error = e; });
    (async () => { for await (const x of g) b.push(x); })().then(() => { }, e => { error = e; });
    drainMicrotasks();
    if (error)
        throw error;
    let all = [...a, ...b].sort((x, y) => x - y);
    let expected = [];
    for (let i = 0; i < 100; ++i)
        expected.push(i);
    shouldBe(all, expected);
    // Disjoint: no value delivered to both loops.
    for (const x of a)
        shouldBe(b.includes(x), false);
})();

// 3. Two `for await` loops that start on an already-completed generator in the same turn both hit
// the completed fast path back-to-back; each must independently observe completion (empty).
(function () {
    async function* gen() { }
    let g = gen();
    (async () => { for await (const _ of g) { } })().then(() => { }, e => { error = e; });
    drainMicrotasks();
    if (error)
        throw error;
    let c1 = 0, c2 = 0;
    (async () => { for await (const _ of g) c1++; })().then(() => { }, e => { error = e; });
    (async () => { for await (const _ of g) c2++; })().then(() => { }, e => { error = e; });
    drainMicrotasks();
    if (error)
        throw error;
    shouldBe(c1, 0);
    shouldBe(c2, 0);
})();
