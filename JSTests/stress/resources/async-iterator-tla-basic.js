// TLA module: exercises the for-await fast consumer at module top level.
const log = [];

// Multi-yield over a pristine async generator -> fast cooperative driving (driver = this module record).
async function* g() { yield 1; yield 2; yield 3; }
for await (const x of g())
    log.push("v" + x);
log.push("done");

// Reconsume an already-exhausted producer: the completed-producer fast path must settle
// { undefined, true } without enqueuing/double-driving the module.
async function* g2() { yield 10; }
const it = g2();
for await (const x of it)
    log.push("a" + x);
for await (const x of it)
    log.push("b" + x); // producer already completed -> loop body never runs
log.push("done2");

// Nested top-level for-await: each inner loop re-registers the module as the driver.
async function* inner(k) { yield k + "a"; yield k + "b"; }
async function* outer() { yield 1; yield 2; }
for await (const o of outer())
    for await (const i of inner(o))
        log.push("n" + i);

export { log };
