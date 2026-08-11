// Regression tests for the SuppressFastResume flag (JSAsyncGenerator / JSAsyncFunctionGenerator per-cell
// bit) under recursion and re-entrancy. The flag is set on a for-await consumer just before it suspends
// at op_yield, and consumed at that suspend, so it must survive: interleaved/queued .next() requests on a
// fast consumer, nested fast for-await in one generator, a producer that re-enters the consumer's .next()
// while the consumer is Executing (must enqueue, not double-drive), reconsuming a completed generator, and
// a manual .next() racing the fast resume. Expected values are the spec-correct results (verified against
// V8); a double-drive or a lost/stale flag would reorder or duplicate them. Warmed to reach the upper JIT
// tiers so the DFG/FTL fast path is exercised too.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

// Four queued .next() requests on an async-generator that itself for-awaits a producer.
async function interleavedNext() {
    async function* prod() { yield 1; yield 2; yield 3; }
    async function* cons(p) { for await (const x of p) yield x * 10; }
    const c = cons(prod());
    const r = await Promise.all([c.next(), c.next(), c.next(), c.next()]);
    return r.map(v => v.value + "/" + v.done).join(" ");
}

// Nested fast for-await inside one consumer generator (each op_async_iterator_next sets the flag, each
// following op_yield consumes it -- the inner and outer loops must not clobber each other's flag).
async function nestedFast() {
    async function* inner(k) { yield k + "a"; yield k + "b"; }
    async function* outer(p) { for await (const x of p) for await (const y of inner(x)) yield y; }
    async function* src() { yield 1; yield 2; }
    let s = "";
    for await (const v of outer(src()))
        s += v + ",";
    return s;
}

// The producer, resumed synchronously by the consumer's fast next, calls the consumer's own .next() while
// the consumer is Executing. That must only enqueue (not re-enter/double-drive) the consumer.
async function reentrantProducer() {
    let cRef;
    async function* prod() { cRef.next(); yield 1; cRef.next(); yield 2; }
    async function* cons(p) { for await (const x of p) yield x * 100; }
    cRef = cons(prod());
    const collected = [];
    for await (const v of cRef) {
        collected.push(v);
        if (collected.length >= 4)
            break;
    }
    return collected.join(",");
}

// Re-consume the same consumer instance with a second loop after it has completed.
async function reconsume() {
    async function* prod() { yield 1; yield 2; }
    async function* cons(p) { for await (const x of p) yield x; yield -1; }
    const c = cons(prod());
    let s = "";
    for await (const v of c) s += "a" + v + ",";
    for await (const v of c) s += "b" + v + ","; // completed -> yields nothing
    return s;
}

// A manual .next() issued while the consumer is mid-await of the producer (races the fast resume).
async function nextDuringAwait() {
    async function* prod() { yield 10; yield 20; yield 30; }
    async function* cons(p) { for await (const x of p) yield x; }
    const c = cons(prod());
    const a = await c.next();
    const pending = c.next();
    const b = await pending;
    const rest = await c.next();
    return [a, b, rest].map(v => v.value + "/" + v.done).join(" ");
}

const cases = [
    [interleavedNext, "10/false 20/false 30/false undefined/true"],
    [nestedFast, "1a,1b,2a,2b,"],
    [reentrantProducer, "100"],
    [reconsume, "a1,a2,a-1,"],
    [nextDuringAwait, "10/false 20/false 30/false"],
];

let done = false;
let error = null;

async function main() {
    const N = testLoopCount; // warm hot enough to reach the upper JIT tiers.
    for (let i = 0; i < N; i++) {
        for (const [fn, expected] of cases) {
            const got = await fn();
            assert(got === expected, fn.name + " iter " + i + ": expected [" + expected + "] got [" + got + "]");
        }
    }
}

main().then(() => { done = true; }, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async main() did not complete");
