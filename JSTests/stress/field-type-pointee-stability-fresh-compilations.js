//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// Pointee stability: a claim names the structure of a different object, which can transition away with nothing
// ever being stored to the claimed field. At record creation the claimed structure's transition watchpoint is
// armed (V8's Map::is_stable), so a transition invalidates the claim and Graph::tryWatch refuses it afterwards.

// Catching a failure needs a compilation that happens AFTER the transition; re-reading through an
// already-compiled function proves nothing, which is why field-type-pointee-transitions-after-claim.js is inert.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Pointee() { this.a = 1; }
function Holder(p) { this.f = p; }

// A fresh reader per round, built with the Function constructor to defeat code-block reuse.
function makeReader(tag) {
    const fn = new Function("h", "return h.f.a + " + tag + " - " + tag + ";");
    noInline(fn);
    return fn;
}

const holders = [];
for (let i = 0; i < 64; ++i)
    holders.push(new Holder(new Pointee()));

// Establish the claim and let a compilation adopt it.
const reader0 = makeReader(0);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(reader0(holders[i & 63]), 1);
gc();

// Transition every pointee away from the claimed structure, storing nothing to h.f.
for (let i = 0; i < 64; ++i)
    holders[i].f.extra = i;

// Brand-new compilations, each of which would adopt the now-stale claim if arming had not invalidated it: a load
// specialised on the old structure reads `extra`'s slot as `a`.
for (let round = 1; round <= 6; ++round) {
    const reader = makeReader(round);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(holders[i & 63]), 1);
    for (let i = 0; i < 64; ++i)
        shouldBe(holders[i].f.extra, i);
    gc();
}

// Transition a second time, and repeat with fresh compilations.
for (let i = 0; i < 64; ++i)
    holders[i].f.second = i * 2;
for (let round = 7; round <= 10; ++round) {
    const reader = makeReader(round);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(reader(holders[i & 63]), 1);
    for (let i = 0; i < 64; ++i) {
        shouldBe(holders[i].f.extra, i);
        shouldBe(holders[i].f.second, i * 2);
    }
    gc();
}

// The function/prototype pair, where the heap verifier first caught this: a prototype object is recorded when
// constructPrototypeObject sets `constructor`.
function makeCtor() {
    function C() { this.v = 7; }
    return C;
}
const ctors = [];
for (let i = 0; i < 64; ++i)
    ctors.push(makeCtor());

const protoReader0 = makeReader(100);
function readCtor(c) { return c.prototype.constructor; }
noInline(readCtor);
for (let i = 0; i < testLoopCount; ++i) {
    const c = ctors[i & 63];
    if (readCtor(c) !== c)
        throw new Error("prototype.constructor mismatch during warmup");
}
gc();
for (let i = 0; i < 64; ++i)
    ctors[i].prototype.mixin = i;
for (let round = 0; round < 4; ++round) {
    const fresh = new Function("c", "return c.prototype.constructor;");
    noInline(fresh);
    for (let i = 0; i < testLoopCount; ++i) {
        const c = ctors[i & 63];
        if (fresh(c) !== c)
            throw new Error("prototype.constructor mismatch after prototype mutation, round " + round);
    }
    for (let i = 0; i < 64; ++i)
        shouldBe(ctors[i].prototype.mixin, i);
    gc();
}
