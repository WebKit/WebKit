// Regression test: iterating a JSStringIterator directly (i.e. the object returned by
// str[Symbol.iterator](), not a primitive string) must not be type-confused with a primitive JSString.
//
// getIterationMode() classifies a primitive string as IterationMode::FastString, and the synchronous
// op_iterator_open fast path builds a JSStringIterator from asString(iterable) on that assumption. If
// getIterationMode also returned FastString for an already-existing JSStringIterator cell, the sync path
// would asString() the iterator itself (an unchecked JSString downcast) and crash. A JSStringIterator is
// therefore left as Generic here (its fast-drive path is intentionally deferred to a separate change);
// this test only pins the invariant that iterating it directly yields the string's code points and never
// crashes, across sync for-of / spread / Array.from and the async-from-sync wrapper, warmed across tiers.

function assert(cond, message) {
    if (!cond)
        throw new Error("Assertion failed: " + message);
}

// Plain sync for-of over a string iterator object (this is the shape that crashed).
function forOfStringIterator(str) {
    const out = [];
    for (const c of str[Symbol.iterator]())
        out.push(c);
    return out.join(",");
}

// Spread of a string iterator (op_iterator_open + op_iterator_next fast path).
function spreadStringIterator(str) {
    return [...str[Symbol.iterator]()].join(",");
}

// Array.from over a string iterator.
function arrayFromStringIterator(str) {
    return Array.from(str[Symbol.iterator]()).join(",");
}

// A partially consumed string iterator: the fast open must REUSE the iterator (not rebuild it from
// scratch), so iteration continues from the current position rather than restarting.
function partiallyConsumed(str) {
    const it = str[Symbol.iterator]();
    it.next(); // drop the first code point
    const out = [];
    for (const c of it)
        out.push(c);
    return out.join(",");
}

let done = false;
let error = null;

// The async-from-sync wrapper drives the same JSStringIterator through the FastStringIterator mode.
async function asyncDriver() {
    for (let i = 0; i < testLoopCount; i++) {
        let out = [];
        for await (const c of "abc"[Symbol.iterator]())
            out.push(c);
        assert(out.join(",") === "a,b,c", "for-await over string iterator: " + out.join(","));

        // A primitive string still classifies as FastString and builds a fresh iterator.
        out = [];
        for await (const c of "xyz")
            out.push(c);
        assert(out.join(",") === "x,y,z", "for-await over primitive string: " + out.join(","));
    }
    done = true;
}

// Warm the sync paths across tiers.
for (let i = 0; i < testLoopCount; i++) {
    assert(forOfStringIterator("abc") === "a,b,c", "for-of string iterator");
    assert(spreadStringIterator("xyz") === "x,y,z", "spread string iterator");
    assert(arrayFromStringIterator("hello") === "h,e,l,l,o", "Array.from string iterator");
    assert(partiallyConsumed("abcd") === "b,c,d", "partially consumed string iterator");
    assert(forOfStringIterator("") === "", "empty string iterator");
    // Astral characters must iterate by code point (2 UTF-16 units), proving genuine string-iterator semantics.
    assert(forOfStringIterator("a\u{1F600}b") === "a,\u{1F600},b", "astral code points");
}

asyncDriver().then(() => {}, (e) => { error = e; });

drainMicrotasks();

if (error)
    throw error;
assert(done, "async driver did not complete");
