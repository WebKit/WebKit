//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1")

// ByVal replace through a pre-compiled shared IC thunk. A shared Replace thunk
// (PutByValWithStringReplaceHandler and friends) has nowhere to bake this field's expected StructureID, so the
// field-type machinery must DECLINE it and fall through to the fully generated handler; otherwise the store is
// unchecked and the claim is never withdrawn. The ById family declines at the equivalent site.
// Unlike the for-in test this needs no isWatchingReplacement dance: the consumer does not depend on the
// property-replacement watchpoint (that shortcut cost 7.4% on delta-blue), which is exactly why a claim can
// coexist with a cached Replace and why the decline has to exist.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Strength() { this.value = 111; }
function Other() { this.a = 1; this.b = 2; this.c = 3; }   // different structure, different layout
function Holder(p) { this.f = p; }

const holders = [];
for (let i = 0; i < 200; ++i)
    holders.push(new Holder(new Strength()));

function read(h) { return h.f.value; }
noInline(read);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(read(holders[i % 200]), 111);
gc();

// Warm the ByVal store on satisfying values, so the claim stays live while the IC settles into a cached Replace.
function storeByVal(o, k, v) { o[k] = v; }
noInline(storeByVal);

const key = "f";
for (let i = 0; i < testLoopCount; ++i)
    storeByVal(holders[i % 200], key, new Strength());
gc();
for (let i = 0; i < 200; ++i)
    shouldBe(holders[i].f.value, 111);

// Store a contradicting value through that same cached ByVal Replace site; the heap walk at the next gc() is the
// real check.
storeByVal(holders[0], key, new Other());
shouldBe(holders[0].f.b, 2);
shouldBe(read(holders[0]), undefined);

gc();

for (let i = 0; i < 200; ++i)
    storeByVal(holders[i], key, new Other());
gc();
for (let i = 0; i < 200; ++i)
    shouldBe(holders[i].f.b, 2);

for (let round = 0; round < 4; ++round) {
    const fresh = new Function("h", "if (h === null) return " + round + "; return h.f.value;");
    noInline(fresh);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(fresh(holders[i % 200]), undefined);
    gc();
}

// A symbol key, which selects PutByValWithSymbolReplaceHandler rather than the string variant.
const sym = Symbol("s");
function SymHolder(p) { this[sym] = p; }
const symHolders = [];
for (let i = 0; i < 200; ++i)
    symHolders.push(new SymHolder(new Strength()));
function readSym(h) { return h[sym].value; }
noInline(readSym);
for (let i = 0; i < testLoopCount; ++i)
    shouldBe(readSym(symHolders[i % 200]), 111);
for (let i = 0; i < testLoopCount; ++i)
    storeByVal(symHolders[i % 200], sym, new Strength());
gc();
for (let i = 0; i < 200; ++i)
    storeByVal(symHolders[i], sym, new Other());
gc();
for (let i = 0; i < 200; ++i)
    shouldBe(symHolders[i][sym].b, 2);
