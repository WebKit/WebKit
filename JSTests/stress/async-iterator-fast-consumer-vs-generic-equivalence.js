// Proves the for-await fast consumer is observably IDENTICAL to the generic (real next.call(iterator))
// path, in the same binary. Each scenario is run twice over the same 8-turn microtask "ruler": first
// with a pristine async generator (op_async_iterator_open stamps the fast sentinel -> fast enqueue),
// then after wrapping %AsyncGeneratorPrototype%.next (breaking the pristine-next identity check so
// op_async_iterator_next takes its generic real-call branch). The wrapper forwards synchronously, so
// any difference in the two logs is a fast-path-only divergence in tick count, ordering, or values.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

async function scenario(body) {
    const log = [];
    const E = s => log.push(s);
    let p = Promise.resolve();
    for (let i = 0; i < 8; i++) {
        const j = i;
        p = p.then(() => E("R" + j));
    }
    await body(E);
    return log.join(",");
}

const scenarios = {
    async gen3(E) {
        async function* g() { yield 1; yield 2; yield 3; }
        for await (const x of g()) E("v" + x);
        E("done");
    },
    async breakEarly(E) {
        async function* g() { try { yield 1; yield 2; yield 3; } finally { E("fin"); } }
        for await (const x of g()) { E("v" + x); if (x === 2) break; }
        E("after");
    },
    async throwInBody(E) {
        async function* g() { try { yield 1; yield 2; } finally { E("fin"); } }
        try { for await (const x of g()) { E("v" + x); if (x === 1) throw new Error("b"); } } catch (e) { E("caught"); }
    },
    async delegate(E) {
        async function* inner() { yield "a"; yield "b"; }
        async function* g() { yield 0; yield* inner(); yield 9; }
        for await (const x of g()) E("v" + x);
    },
    async internalAwait(E) {
        async function* g() { await 0; yield 1; await 0; yield 2; }
        for await (const x of g()) E("v" + x);
    },
    async yieldPromise(E) {
        async function* g() { yield Promise.resolve("p"); yield "q"; }
        for await (const x of g()) E("v" + x);
    },
    async nested(E) {
        async function* inner(k) { yield k + "a"; yield k + "b"; }
        async function* outer() { yield 1; yield 2; }
        for await (const o of outer()) for await (const i of inner(o)) E("v" + i);
    },
    async errorFromProducer(E) {
        async function* g() { yield 1; throw new Error("producer"); }
        try { for await (const x of g()) E("v" + x); } catch (e) { E("caught:" + e.message); }
    },
    async agConsumer(E) {
        // Async-generator consumer (SuppressFastResume path) driven by an outer for-await.
        async function* producer() { yield 1; yield 2; }
        async function* consumer() { for await (const x of producer()) yield x * 10; }
        for await (const y of consumer()) E("y" + y);
    },
};

const names = Object.keys(scenarios);

async function runAll() {
    const out = {};
    for (const name of names)
        out[name] = await scenario(scenarios[name]);
    return out;
}

const asyncGenProto = Object.getPrototypeOf(Object.getPrototypeOf((async function* () {})()));

let done = false;
let error = null;

async function main() {
    assert(Object.prototype.hasOwnProperty.call(asyncGenProto, "next"), "found %AsyncGeneratorPrototype%.next");

    const fast = await runAll();

    // Break the pristine-next identity so the fast sentinel is never installed -> generic real-call path.
    const origNext = asyncGenProto.next;
    asyncGenProto.next = function (...args) { return origNext.apply(this, args); };

    const generic = await runAll();

    for (const name of names)
        assert(fast[name] === generic[name],
            name + "\n  fast:    " + fast[name] + "\n  generic: " + generic[name]);
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
