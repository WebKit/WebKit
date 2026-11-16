//@ skip if $secAccel3Removed
//@ requireOptions("--sweepSynchronously=1", "--zeroExecutableMemoryOnFree=1")

// A DFG/FTL DirectTailCall to a NoIntrinsic host function used to emit an
// inline C call after prepareForTailCall destroyed the caller's frame. The
// pushed return address pointed into the caller's own JIT code, which nothing
// rooted: the conservative scan does not treat return addresses into CodeBlock
// JIT code as roots, and the shuffled frame's codeBlock slot is nullptr. A GC
// inside the host call that found one of the caller's weak references dead
// would jettison and (with --sweepSynchronously) free the caller's machine
// code, so the host function's return landed in freed JIT memory.
//
// The tail caller must have argCountIncludingThis <= the host callee's frame
// (here both are 2), so that the shuffle fully overwrites the old frame header
// and no stale codeBlock-slot word conservatively roots the caller.

"use strict";

globalThis.sideSink = 0;
let sink = null;

// Overwrite stale stack words that might conservatively root the helper.
function scrub(d) {
    let a = d + 0.5, b = d * 1.5, c = d - 0.25, e = d ^ 7;
    return d > 0 ? scrub(d - 1) + a + b + c + e : a;
}
noInline(scrub);

// Invalidate the global's inferred-value watchpoint so that nulling it later
// does not jettison the caller via watchpoint instead of via GC.
globalThis.h = function d1() {};
globalThis.h = function d2() {};

for (let gen = 0; gen < 50; gen++) {
    // Fresh executables each generation so a jettisoned code block does not
    // count against reoptimization heuristics of the next round.
    const tail = new Function('n', `//g${gen}
        "use strict";
        if (n & 1) globalThis.sideSink = globalThis.h(n);
        return "a".repeat(n);`);
    noInline(tail);
    globalThis.h = new Function('n', `//h${gen}
        return n | 0;`);

    for (let i = 0; i < 4000; i++)
        sink = tail(2 + (i & 1));

    globalThis.h = null; // Kill the frozen weak reference of tail's DFG code.
    globalThis.scrub2 = scrub(2000);

    // Even n skips the helper branch. Each call allocates megabytes inside
    // stringProtoFuncRepeat, so a full GC lands inside the host call while
    // the tail-shuffled frame no longer roots the caller's CodeBlock.
    for (let k = 0; k < 16; k++)
        sink = tail((1 << 20) + (k << 18));
}

if (sink.length !== (1 << 20) + (15 << 18))
    throw new Error("bad result: " + sink.length);
