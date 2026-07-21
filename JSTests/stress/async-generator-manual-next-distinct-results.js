// Regression test: the async generator driver's per-generator iterator-result object cache must be
// used ONLY on the internal cooperative for-await driver path (where the result is unobservable).
// A manual .next()/.throw()/.return() settles a real user-visible Promise, so per spec each call
// must resolve to a distinct CreateIteratorResultObject. This guards against the cache leaking into
// the manual path (which would make two manual next() calls resolve to the same, aliased object).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: expected ${expected} but got ${actual}`);
}

function shouldBeDistinct(objs) {
    shouldBe(new Set(objs).size, objs.length);
}

let error = null;

// A. Sequential manual next(): each resolves to a distinct object with the right value.
(function () {
    async function* gen() {
        for (let i = 0; i < 100; ++i)
            yield i;
    }

    let results = [];
    (async function () {
        let it = gen();
        for (let i = 0; i < 102; ++i)
            results.push(await it.next());
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBe(results.length, 102);
    shouldBeDistinct(results);
    for (let i = 0; i < 100; ++i) {
        shouldBe(results[i].value, i);
        shouldBe(results[i].done, false);
    }
    shouldBe(results[100].done, true);
    shouldBe(results[101].done, true);
})();

// B. Concurrent manual next() (all queued before any settles): distinct objects, ordered values.
(function () {
    async function* gen() {
        yield 10;
        yield 20;
        yield 30;
    }

    let results = null;
    (async function () {
        let it = gen();
        results = await Promise.all([it.next(), it.next(), it.next(), it.next()]);
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBeDistinct(results);
    shouldBe(results[0].value, 10);
    shouldBe(results[1].value, 20);
    shouldBe(results[2].value, 30);
    shouldBe(results[3].done, true);
})();

// C. Repeated next() past completion: each is a distinct { value: undefined, done: true }.
(function () {
    async function* gen() {
        yield 1;
    }

    let results = [];
    (async function () {
        let it = gen();
        await it.next();
        for (let i = 0; i < 5; ++i)
            results.push(await it.next());
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBeDistinct(results);
    for (const r of results) {
        shouldBe(r.done, true);
        shouldBe(r.value, undefined);
    }
})();

// D. Mixing cooperative driving with manual next() on the same generator: manual results stay fresh.
(function () {
    async function* gen() {
        for (let i = 0; i < 6; ++i)
            yield i;
    }

    let manual = [];
    (async function () {
        let it = gen();

        // Drive a couple of steps cooperatively (this populates the internal cache).
        for await (const v of (async function* () { yield* it; })()) {
            if (v === 1)
                break;
        }

        // Now pull the remaining steps manually; these must be distinct objects.
        for (let i = 0; i < 3; ++i)
            manual.push(await it.next());
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBeDistinct(manual);
})();

// E. .return() and .throw() also resolve to distinct objects.
(function () {
    async function* gen() {
        yield 1;
        yield 2;
    }

    let results = [];
    (async function () {
        let it = gen();
        results.push(await it.next());
        results.push(await it.return(42));
        results.push(await it.next());
    })().then(() => { }, e => { error = e; });

    drainMicrotasks();
    if (error)
        throw error;

    shouldBeDistinct(results);
    shouldBe(results[0].value, 1);
    shouldBe(results[1].value, 42);
    shouldBe(results[1].done, true);
    shouldBe(results[2].done, true);
})();
