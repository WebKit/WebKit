//@ requireOptions("--useFieldTypeAssumptions=1", "--validateFieldTypes=1", "--useDollarVM=1")

// Store-side withdrawal of a claim, driven through every channel a replace store can take. A replace store writes
// a field that already exists, so no creation hook sees it, and the verifier fails on any live object left holding
// a value that contradicts the claim.

// field-type-replace-store-through-jit.js drives only one monomorphic JIT site, whose inline check covers for
// missing C++ maintenance, so the two mechanisms mask each other there.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: expected " + expected + " but got " + actual);
}

function Claimed(a) { this.a = a; }
function Contradicting(a) { this.zz = 0; this.a = a * 3; }

function makeHolder(v) { const o = {}; o.f = v; return o; }
function readF(o) { return o.f.a; }
noInline(readF);

function freshPopulation(n) {
    const out = [];
    for (let i = 0; i < n; ++i)
        out.push(makeHolder(new Claimed(i)));
    return out;
}
function warmUp(pop) {
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i % pop.length]), i % pop.length);
}

// Channel 1: ordinary put_by_id replace from a non-inlined function.
{
    const pop = freshPopulation(64);
    warmUp(pop);
    gc();
    function replace(o, v) { o.f = v; }
    noInline(replace);
    for (let i = 0; i < 64; ++i)
        replace(pop[i], new Contradicting(i));
    gc();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i & 63]), (i & 63) * 3);
}

// Channel 2: Object.defineProperty, which reaches putDirectInternal through validateAndApplyPropertyDescriptor.
{
    const pop = freshPopulation(64);
    warmUp(pop);
    gc();
    for (let i = 0; i < 64; ++i)
        Object.defineProperty(pop[i], "f", { value: new Contradicting(i) });
    gc();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i & 63]), (i & 63) * 3);
}

// Channel 3: Reflect.set, a different C++ entry into the same slot.
{
    const pop = freshPopulation(64);
    warmUp(pop);
    gc();
    for (let i = 0; i < 64; ++i)
        Reflect.set(pop[i], "f", new Contradicting(i));
    gc();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i & 63]), (i & 63) * 3);
}

// Channel 4: a megamorphic put site, which caches differently and so makes a different decline/poison decision.
{
    const pop = freshPopulation(64);
    warmUp(pop);
    gc();
    const shapes = [];
    for (let i = 0; i < 16; ++i) {
        const o = {};
        o["k" + i] = i;      // 16 distinct shapes so the site below goes megamorphic
        o.f = new Claimed(i);
        shapes.push(o);
    }
    function megaStore(o, v) { o.f = v; }
    noInline(megaStore);
    for (let i = 0; i < testLoopCount; ++i)
        megaStore(shapes[i & 15], new Claimed(i & 15));
    for (let i = 0; i < 64; ++i)
        megaStore(pop[i], new Contradicting(i));
    for (let i = 0; i < 16; ++i)
        megaStore(shapes[i], new Contradicting(i));
    gc();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i & 63]), (i & 63) * 3);
    for (let i = 0; i < 16; ++i)
        shouldBe(shapes[i].f.a, i * 3);
}

// Channel 5: a store through a computed key, i.e. put_by_val rather than put_by_id.
{
    const pop = freshPopulation(64);
    warmUp(pop);
    gc();
    const key = "f";
    function valStore(o, k, v) { o[k] = v; }
    noInline(valStore);
    for (let i = 0; i < 64; ++i)
        valStore(pop[i], key, new Contradicting(i));
    gc();
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(readF(pop[i & 63]), (i & 63) * 3);
}

// Channel 6: replace while the field is read polymorphically, so both structures flow through one MultiGetByOffset.
{
    const claimedPop = freshPopulation(32);
    const otherPop = [];
    for (let i = 0; i < 32; ++i) {
        const o = {};
        o.pad = i;             // a different shape, same property name
        o.f = new Claimed(i);
        otherPop.push(o);
    }
    warmUp(claimedPop);
    for (let i = 0; i < testLoopCount; ++i)
        shouldBe(otherPop[i & 31].f.a, i & 31);
    gc();
    for (let i = 0; i < 32; ++i) {
        claimedPop[i].f = new Contradicting(i);
        otherPop[i].f = new Contradicting(i);
    }
    gc();
    for (let i = 0; i < testLoopCount; ++i) {
        const idx = i & 31;
        shouldBe(readF(claimedPop[idx]), idx * 3);
        shouldBe(otherPop[idx].f.a, idx * 3);
    }
}
