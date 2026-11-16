// Coverage for op_async_iterator_open/next under abrupt completion and reuse, warmed enough to tier the
// for-await site to the upper JITs. Repeats break / return / throw (each triggers AsyncIteratorClose ->
// the generator's return(), so finally must run), nested for-await, continue, partial manual consume
// before for-await, and a producer that throws. Each scenario returns an event log asserted against the
// exact expected sequence, so ordering, finally timing, and values are all pinned across tiers.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message + " got: " + JSON.stringify(cond));
}

function assertSeq(actual, expected, name) {
    const a = actual.join(",");
    const e = expected.join(",");
    if (a !== e)
        throw new Error(name + " sequence mismatch:\n  actual:   " + a + "\n  expected: " + e);
}

async function breakEarly() {
    const log = [];
    async function* g() { try { yield 1; yield 2; yield 3; } finally { log.push("fin"); } }
    for await (const x of g()) { log.push("v" + x); if (x === 2) break; }
    log.push("after");
    return log;
}

async function throwInBody() {
    const log = [];
    async function* g() { try { yield 1; yield 2; } finally { log.push("fin"); } }
    try {
        for await (const x of g()) { log.push("v" + x); if (x === 1) throw new Error("boom"); }
    } catch (e) { log.push("caught:" + e.message); }
    return log;
}

async function returnFromEnclosing() {
    const log = [];
    async function* g() { try { yield 1; yield 2; yield 3; } finally { log.push("fin"); } }
    async function inner() {
        for await (const x of g()) { log.push("v" + x); if (x === 2) return "early"; }
        log.push("fellThrough");
    }
    log.push("ret:" + await inner());
    return log;
}

async function continueInBody() {
    const log = [];
    async function* g() { yield 1; yield 2; yield 3; yield 4; }
    for await (const x of g()) { if (x % 2 === 0) continue; log.push("v" + x); }
    return log;
}

async function nested() {
    const log = [];
    async function* inner(k) { yield k + "a"; yield k + "b"; }
    async function* outer() { yield 1; yield 2; }
    for await (const o of outer())
        for await (const i of inner(o)) log.push("v" + i);
    return log;
}

async function partialThenForAwait() {
    const log = [];
    async function* g() { yield 1; yield 2; yield 3; }
    const it = g();
    const first = await it.next(); // manual drive one step
    log.push("manual:" + first.value);
    for await (const x of it) log.push("v" + x); // for-await consumes the rest via the same iterator
    return log;
}

async function producerThrows() {
    const log = [];
    async function* g() { yield 1; yield 2; throw new Error("producer"); }
    try {
        for await (const x of g()) log.push("v" + x);
    } catch (e) { log.push("caught:" + e.message); }
    return log;
}

const cases = [
    [breakEarly, ["v1", "v2", "fin", "after"]],
    [throwInBody, ["v1", "fin", "caught:boom"]],
    [returnFromEnclosing, ["v1", "v2", "fin", "ret:early"]],
    [continueInBody, ["v1", "v3"]],
    [nested, ["v1a", "v1b", "v2a", "v2b"]],
    [partialThenForAwait, ["manual:1", "v2", "v3"]],
    [producerThrows, ["v1", "v2", "caught:producer"]],
];

let error = null;
let done = false;

async function main() {
    for (let k = 0; k < testLoopCount; k++) {
        for (const [fn, expected] of cases)
            assertSeq(await fn(), expected, fn.name);
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
