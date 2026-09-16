//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// A claim names the structure of a different object, which can transition away with nothing stored to the claimed
// field, so no store path observes it: only the transition watchpoint armed at record creation can invalidate the
// claim. This file re-reads through already-compiled functions and is therefore inert; the version with teeth is
// field-type-pointee-stability-fresh-compilations.js.

function Pointee() { this.a = 1; }
function Holder(p) { this.f = p; }

function readThroughField(h) { return h.f.a; }
noInline(readThroughField);

const holders = [];
for (let i = 0; i < 64; ++i)
    holders.push(new Holder(new Pointee()));

for (let i = 0; i < testLoopCount; ++i) {
    if (readThroughField(holders[i & 63]) !== 1)
        throw new Error("bad read during warmup");
}

// Transition every pointee away from the claimed structure, without storing to h.f at all.
for (let i = 0; i < 64; ++i)
    holders[i].f.extra = i;

for (let i = 0; i < testLoopCount; ++i) {
    if (readThroughField(holders[i & 63]) !== 1)
        throw new Error("bad read after the pointee transitioned");
}

// The newly added property must be readable, so the load must not be specialised on the old structure.
for (let i = 0; i < 64; ++i) {
    if (holders[i].f.extra !== i)
        throw new Error("expected extra=" + i + ", got " + holders[i].f.extra);
}

// The function/prototype pair: A.prototype is recorded when constructPrototypeObject sets `constructor`.
function makeCtor() {
    function C() { this.v = 7; }
    return C;
}
function readProtoConstructor(c) { return c.prototype.constructor; }
noInline(readProtoConstructor);

const ctors = [];
for (let i = 0; i < 64; ++i)
    ctors.push(makeCtor());
for (let i = 0; i < testLoopCount; ++i) {
    const c = ctors[i & 63];
    if (readProtoConstructor(c) !== c)
        throw new Error("prototype.constructor mismatch during warmup");
}
// Mutate the prototypes, transitioning them away from the recorded structure.
for (let i = 0; i < 64; ++i)
    ctors[i].prototype.mixin = i;
for (let i = 0; i < testLoopCount; ++i) {
    const c = ctors[i & 63];
    if (readProtoConstructor(c) !== c)
        throw new Error("prototype.constructor mismatch after prototype mutation");
}
