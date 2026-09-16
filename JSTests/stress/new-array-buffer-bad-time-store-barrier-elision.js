//@ skip if $secAccel3Removed
//@ runDefault("--verifyGC=1", "--collectContinuously=1", "--forceEagerCompilation=1", "--useFTLJIT=0")

// --useFTLJIT=0 pins f to the DFG. FTL elides the same barrier, but both tiers reach the bug
// through the same operationNewArrayBuffer, so testing one tier is enough.
//
// When the global object is having a bad time, operationNewArrayBuffer creates the JSArray cell and
// then allocates its ArrayStorage butterfly via switchToSlowPutArrayStorage. That second allocation
// must not let a GC run: a conservative stack scan would blacken the freshly created array, which
// breaks the invariant DFG's StoreBarrierInsertionPhase relies on when it elides the
// FencedStoreBarrier for stores into a cell allocated in the current epoch. The store below would
// then create an untracked black -> white edge and the victim would be swept while reachable.

// Not testLoopCount: these iterations exist to accumulate GC-cycle race attempts, not only to reach a tier.
const D = 130, W = 55, K = 100, LIT = 4096, ITER = 1000;

function clobber(a, b, c, d, e, g, h, i) { return a ^ b ^ c ^ d ^ e ^ g ^ h ^ i; }
noInline(clobber);

var mkTail, mkLink;
{
    let props = [];
    for (let j = 0; j < W; ++j)
        props.push("b" + j + ": {}");
    mkTail = eval("(function (victim) { return { a: null, p: victim, " + props.join(", ") + " }; })");
    mkLink = eval("(function (next) { return { a: next, " + props.join(", ") + " }; })");
}

function makeChain() {
    let victim = {};
    let cur = mkTail(victim);
    for (let i = 0; i < D; ++i)
        cur = mkLink(cur);
    return cur;
}
noInline(makeChain);

// Zeroes out the dead frames of makeChain()/f() so that a stale spill slot doesn't keep the victim
// alive as a conservative root.
var scrub;
{
    let parts = [];
    for (let i = 0; i < 512; ++i)
        parts.push("a" + i + " = 0");
    scrub = eval("(function scrub() { let " + parts.join(", ") + "; return a0 + a511; })");
    noInline(scrub);
}

// A straight-line DFG function with no GC point after its NewArrayBuffer, so it never safepoints
// while the concurrent marker drains.
let source = "(obj, seed) => {\n";
source += "    let arr = [" + Array(LIT).fill(7).join(",") + "];\n";
source += "    let x = seed | 0;\n";
for (let i = 0; i < K; ++i)
    source += "    x = (x ^ (x << 13)); x = (x ^ (x >>> 17)); x = (x ^ (x << 5));\n";
let cur = "obj";
for (let i = 0; i < D; ++i) {
    source += "    let v" + i + " = " + cur + ".a;\n";
    cur = "v" + i;
}
source += "    let victim = " + cur + ".p;\n";
source += "    arr[0] = victim;\n";      // The store whose barrier was elided.
source += "    " + cur + ".p = null;\n"; // Drop the only other reference to the victim.
source += "    victim = null;\n";
for (let i = D - 1; i >= 0; --i)
    source += "    v" + i + " = null;\n";
source += "    clobber(x, x, x, x, x, x, x, x);\n";
source += "    return arr;\n";
source += "}";
const f = eval(source);

// Force a bad time so that NewArrayBuffer takes the slow-put-array-storage path.
Object.defineProperty(Array.prototype, 0, { get() { } });

let keepArr = null;
let seed = 0x2545F491;
for (let i = 0; i < ITER; ++i) {
    let head = makeChain();
    scrub();
    keepArr = f(head, seed + i);
    scrub();
}

if (keepArr.length !== LIT)
    throw new Error("bad length: " + keepArr.length);
