// Verifies top-level-await (TLA) modules can cooperatively drive a native async generator via the
// for-await fast consumer path (op_async_iterator_open fast sentinel -> op_async_iterator_next
// driver branch -> AsyncGeneratorDriverResume with the module record as driver). Before the
// unification, module for-await always took the generic real-call branch. These checks confirm the
// fast path at module scope produces correct values/order, does not double-drive completed
// producers, handles nesting, and propagates producer errors to the module's top-level capability.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

function assertArrayEq(actual, expected, message) {
    assert(Array.isArray(actual), message + " (not an array: " + actual + ")");
    assert(actual.length === expected.length, message + "\n  expected: " + expected.join(",") + "\n  got:      " + actual.join(","));
    for (let i = 0; i < expected.length; i++)
        assert(actual[i] === expected[i], message + " at [" + i + "]\n  expected: " + expected.join(",") + "\n  got:      " + actual.join(","));
}

let pending = 0;
let error = null;

function track(promise, onValue) {
    pending++;
    promise.then((v) => { onValue(v); pending--; }, (e) => { error = e; pending--; });
}

// 1. Basic multi-yield, completed-producer reconsume (no double-drive), and nested TLA for-await.
track(import("./resources/async-iterator-tla-basic.js"), (ns) => {
    assertArrayEq(ns.log, ["v1", "v2", "v3", "done", "a10", "done2", "n1a", "n1b", "n2a", "n2b"], "basic TLA fast-consumer log");
});

// 2. Producer that throws must reject the module's top-level capability.
pending++;
import("./resources/async-iterator-tla-error.js").then(
    () => { error = new Error("error module should have rejected"); pending--; },
    (e) => { assert(e instanceof Error && e.message === "boom", "producer error propagated, got: " + e); pending--; });

// 3. Fast (pristine async generator) vs generic (tampered %AsyncGeneratorPrototype%.next) at module
//    scope must be observably identical -- same values, ordering, and microtask tick cadence.
pending++;
import("./resources/async-iterator-tla-ruler-fast.js")
    .then(() => import("./resources/async-iterator-tla-ruler-generic.js"))
    .then(() => {
        const fast = globalThis.__tlaRulerFast;
        const generic = globalThis.__tlaRulerGeneric;
        assert(typeof fast === "string" && fast.length, "fast ruler log present");
        assert(fast === generic, "TLA fast vs generic ruler\n  fast:    " + fast + "\n  generic: " + generic);
        pending--;
    }, (e) => { error = e; pending--; });

drainMicrotasks();

if (error)
    throw error;
assert(pending === 0, "not all TLA module imports settled (pending=" + pending + ")");
