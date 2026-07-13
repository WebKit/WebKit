function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`bad value: ${actual}, expected ${expected}`);
}

// When the sync iterator yields a rejected value promise, AsyncFromSyncIteratorContinuation
// performs IteratorClose(syncIteratorRecord, ThrowCompletion(error)). Any exception thrown
// while getting or calling the return method must be discarded and the promise must be
// rejected with the original rejection reason (ECMA-262 IteratorClose steps 3-6).

// return() throws.
{
    let state = "pending";
    const syncIterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(new Error("original")), done: false }; },
                return() { throw new Error("close error"); }
            };
        }
    };
    (async () => {
        try {
            for await (const x of syncIterable) {}
            state = "fulfilled";
        } catch (e) {
            state = e.message;
        }
    })();
    drainMicrotasks();
    shouldBe(state, "original");
}

// 'return' getter throws.
{
    let state = "pending";
    const syncIterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(new Error("original")), done: false }; },
                get return() { throw new Error("getter error"); }
            };
        }
    };
    (async () => {
        try {
            for await (const x of syncIterable) {}
            state = "fulfilled";
        } catch (e) {
            state = e.message;
        }
    })();
    drainMicrotasks();
    shouldBe(state, "original");
}

// 'return' is non-callable: the GetMethod TypeError is also discarded.
{
    let state = "pending";
    const syncIterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(new Error("original")), done: false }; },
                return: 42
            };
        }
    };
    (async () => {
        try {
            for await (const x of syncIterable) {}
            state = "fulfilled";
        } catch (e) {
            state = e.message;
        }
    })();
    drainMicrotasks();
    shouldBe(state, "original");
}

// return() returns normally: iterator is closed exactly once, original reason preserved.
{
    let state = "pending";
    let returnCount = 0;
    const syncIterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(new Error("original")), done: false }; },
                return() { returnCount++; return { done: true }; }
            };
        }
    };
    (async () => {
        try {
            for await (const x of syncIterable) {}
            state = "fulfilled";
        } catch (e) {
            state = e.message;
        }
    })();
    drainMicrotasks();
    shouldBe(state, "original");
    shouldBe(returnCount, 1);
}

// Same via async generator yield* delegation.
{
    let state = "pending";
    const syncIterable = {
        [Symbol.iterator]() {
            return {
                next() { return { value: Promise.reject(new Error("original")), done: false }; },
                return() { throw new Error("close error"); }
            };
        }
    };
    async function* gen() { yield* syncIterable; }
    gen().next().then(
        () => { state = "fulfilled"; },
        (e) => { state = e.message; });
    drainMicrotasks();
    shouldBe(state, "original");
}
