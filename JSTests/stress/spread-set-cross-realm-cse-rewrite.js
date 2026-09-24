//@ skip if $secAccel3Removed

// Spread(SetObjectUse) must not fold its structure check against a realm whose Set
// iterator-protocol watchpoint was never armed.
//
// Deriving the realm from Spread->child1()->origin.semantic is not enough, because that edge is not
// stable across phases. At FixupPhase child1() is a GetLocal reading the operand at the [...s] site
// in realm A, so realm A is what gets armed. CFGSimplification then merges the operand block and
// LocalCSE/CPSRethreading forward the GetLocal to the realm-B JSConstant produced inside inlined
// innerB(), so the guarded CFA reads realm B and folds {realmB.setStructure} against itself. The
// post-Spread CheckStructure is then eliminated while only realm A was ever watched.
//
// Overriding realmB.Set.prototype[Symbol.iterator] reaches operationSpreadSet's generic path, whose
// iterator transitions the ArrayWithDouble victim to ArrayWithContiguous. With the CheckStructure
// gone, the following double-typed GetByVal reads the stored JSCell* as raw double bits.
//
// Both lowerings are covered without pinning a tier here: run-jsc-stress-tests runs every test in
// DFG-only configurations (no-ftl) as well as FTL ones (ftl-eager and friends).

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

const realmB = createGlobalObject();
// `const` so the DFG constant-infers the global lexical and inlining innerB() leaves a realm-B
// JSConstant for LocalCSE to forward the GetLocal to.
realmB.eval(`
    const theSet = new Set();
    globalThis.innerB = function () { return theSet; };
`);
globalThis.innerB = realmB.innerB;

// Installed before warmup: realm B's watchpoint fires now, realm A's stays valid, so a guard that
// consults realm A at FixupPhase sees nothing wrong.
let attacking = false;
let victim = null;
const payload = { toString() { return "P"; } };
const emptyIterator = { next() { return { done: true, value: undefined }; } };
realmB.Set.prototype[Symbol.iterator] = function () {
    if (attacking)
        victim[0] = payload;
    return emptyIterator;
};

// Spoil ArrayWithDouble's transition watchpoint before any warmup. Otherwise the first CFA's
// clobberWorld() installs a structure watchpoint that OSR-exits at the post-Spread
// InvalidationPoint when the victim transitions, masking the fold.
{
    const spoiler = [1.1, 2.2, 3.3, 4.4];
    spoiler.push(5.5); // Non-CoW ArrayWithDouble, matching makeDoubleArray()'s structure.
    spoiler[0] = {};
}

function makeDoubleArray() {
    const a = [];
    a.push(1.1);
    a.push(2.2);
    a.push(3.3);
    a.push(4.4);
    return a;
}
noInline(makeDoubleArray);

// The branch on arr is what lets CFGSimplification merge the operand block, which is what exposes
// the GetLocal to LocalCSE. Multiplying by 0.0 keeps the trailing read double-typed.
function leak(arr) {
    const x = arr[0];
    const s = innerB();
    if (arr) {
        const t = [...s];
        return arr[0] + x * 0.0;
    }
    return -1;
}
noInline(leak);

const warm = makeDoubleArray();
for (let i = 0; i < testLoopCount; ++i)
    leak(warm);

attacking = true;
victim = makeDoubleArray();
const result = leak(victim);
attacking = false;

// The iterator stored an object into the victim, so the read has to observe an object. A number
// here means the JSCell* came back as raw double bits.
shouldBe(typeof result, "string");
shouldBe(result, "P0");
