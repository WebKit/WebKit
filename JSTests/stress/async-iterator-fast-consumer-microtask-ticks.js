// Verifies the for-await fast consumer consumes the EXACT number of microtask turns per step that the
// spec (and V8/Node) require -- no more, no fewer. Each scenario launches a fixed 8-turn background
// microtask "ruler" before the consumer; the interleaving of ruler ticks (Rn) with consumer/producer
// events makes the per-step tick cadence observable. The expected sequences below were captured from
// V8 (Node) and are the spec-correct ground truth; the fast consumer must reproduce them byte-for-byte.
// A drifted tick count (e.g. an extra/missing await turn) reorders these and fails.

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
    async break(E) {
        async function* g() { try { yield 1; yield 2; yield 3; } finally { E("fin"); } }
        for await (const x of g()) { E("v" + x); if (x === 2) break; }
        E("after");
    },
    async throw(E) {
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
    async empty(E) {
        async function* g() { }
        for await (const x of g()) E("v" + x);
        E("done");
    },
    async completedReconsume(E) {
        async function* g() { yield 1; }
        const it = g();
        for await (const x of it) E("a" + x);
        for await (const x of it) E("b" + x); // completed
        E("done");
    },
};

// Spec-correct interleavings (captured from V8 / Node).
const expected = {
    gen3: "R0,R1,v1,R2,R3,v2,R4,R5,v3,R6,done,R7",
    break: "R0,R1,v1,R2,R3,v2,R4,fin,R5,after,R6",
    throw: "R0,R1,v1,R2,fin,R3,caught,R4",
    delegate: "R0,R1,v0,R2,R3,R4,va,R5,R6,R7,vb,v9",
    internalAwait: "R0,R1,R2,v1,R3,R4,R5,v2,R6,R7",
    yieldPromise: "R0,R1,vp,R2,R3,vq,R4,R5",
    nested: "R0,R1,R2,R3,v1a,R4,R5,v1b,R6,R7,v2a,v2b",
    empty: "R0,done,R1",
    completedReconsume: "R0,R1,a1,R2,R3,done,R4",
};

let done = false;
let error = null;

async function main() {
    for (const name of Object.keys(expected)) {
        const got = await scenario(scenarios[name]);
        assert(got === expected[name], name + "\n  expected: " + expected[name] + "\n  got:      " + got);
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
