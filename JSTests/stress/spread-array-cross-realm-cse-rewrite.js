//@ skip if $secAccel3Removed

// The Array analogue of spread-set-cross-realm-cse-rewrite.js.
//
// Spread(ArrayUse)'s fast path copies the butterfly directly, bypassing Symbol.iterator, and its
// runtime structure check compares against a realm's primordial array structures. That check only
// establishes the operand's shape - it does not establish that the realm it belongs to still has a
// primordial iteration protocol, which is what the arrayIteratorProtocol watchpoint is for. So the
// realm compared against and the realm armed have to be the same one.
//
// They are not when the realm is re-derived from Spread->child1()->origin.semantic: FixupPhase arms
// realm A via the GetLocal at the [...a] site, then LocalCSE forwards that edge to the realm-B
// JSConstant from inlined innerB() and the lowering compares against realm B's structures. Patching
// realmB.Array.prototype[Symbol.iterator] then leaves the fast path copying the butterfly while the
// correct answer is whatever the patched iterator yields.

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " (expected " + expected + ")");
}

const realmB = createGlobalObject();
realmB.eval(`
    const theArray = [1.1, 2.2, 3.3];
    globalThis.innerB = function () { return theArray; };
`);
globalThis.innerB = realmB.innerB;

// Patched before warmup, so realm B's watchpoint is already invalid while realm A's stays valid.
// A reused one-shot iterator rather than a generator, to keep the warmup loop cheap.
let yielded = false;
const oneShot = {
    next() {
        if (yielded)
            return { done: true, value: undefined };
        yielded = true;
        return { done: false, value: 42 };
    }
};
realmB.Array.prototype[Symbol.iterator] = function () {
    yielded = false;
    return oneShot;
};

// Reading from arr first proves it an object, so CFA proves the branch taken and
// CFGSimplification merges the blocks - which is what lets LocalCSE forward the [...a] operand from
// a GetLocal to the realm-B JSConstant, after FixupPhase has already armed realm A.
function spreadIt(arr) {
    const x = arr[0];
    const a = innerB();
    if (arr) {
        const t = [...a];
        return t.length + x * 0.0;
    }
    return -1;
}
noInline(spreadIt);

function makeDoubleArray() {
    const a = [];
    a.push(1.1);
    a.push(2.2);
    a.push(3.3);
    return a;
}
noInline(makeDoubleArray);

const warm = makeDoubleArray();
for (let i = 0; i < testLoopCount; ++i)
    spreadIt(warm);

// The patched iterator yields exactly one value. Getting realm B's three butterfly elements back
// instead means the fast path ran against a realm whose iteration protocol was never watched.
shouldBe(spreadIt(warm), 1);
