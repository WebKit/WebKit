//@ requireOptions("--useAsyncStackTrace=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`expected "${expected}" but got "${actual}"`);
}

let stack;
async function leaf() {
    await 0;
    await 0;
    stack = new Error().stack;
}

function asyncFrames(run) {
    stack = undefined;
    run();
    drainMicrotasks();
    return stack.split("\n").filter(line => line.startsWith("async ")).map(line => line.replace(/@.*/, "")).join();
}

for (const combinator of ["all", "allSettled", "any", "race"]) {
    shouldBe(asyncFrames(() => {
        async function a() { await 0; await Promise[combinator]([leaf(), promise]); }
        const promise = a();
    }), "async a");

    shouldBe(asyncFrames(() => {
        async function a() { await 0; await Promise[combinator]([promise, leaf()]); }
        async function b() { await a(); }
        const promise = b();
    }), "async a,async b");

    shouldBe(asyncFrames(() => {
        async function a() { await 0; await Promise[combinator]([c(), promise]); }
        async function b() { await a(); }
        async function c() { await leaf(); }
        const promise = b();
    }), "async c,async a,async b");

    shouldBe(asyncFrames(() => {
        async function a() { await 0; await Promise[combinator]([b(), promise]); }
        async function b() { await Promise[combinator]([leaf(), promise]); }
        const promise = a();
    }), "async b,async a");

    shouldBe(asyncFrames(() => {
        const promise = Array.fromAsync({ length: 2, 0: 0, get 1() { return Promise[combinator]([leaf(), promise]); } });
    }), "");

    shouldBe(asyncFrames(() => {
        async function a() { await Array.fromAsync({ length: 2, 0: 0, get 1() { return Promise[combinator]([leaf(), promise]); } }); }
        const promise = a();
    }), "async a");

    shouldBe(asyncFrames(() => {
        async function a(depth) { await Promise[combinator]([depth ? a(depth - 1) : leaf()]); }
        a(3);
    }), "async a,async a,async a,async a");
}

Error.stackTraceLimit = Infinity;
shouldBe(asyncFrames(() => {
    async function a() { await 0; await Promise.all([leaf(), promise]); }
    const promise = a();
}), "async a");
