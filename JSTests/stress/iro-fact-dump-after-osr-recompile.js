//@ skip if !$isFTLPlatform
//@ runDefault("--useTestingHelpers=1", "--useDollarVM=1", "--useConcurrentJIT=0", "--useLoopUnrolling=0")

// The dump map is keyed by CodeBlock*, so after a recompile $vm.iroFactDump
// must follow fn->replacement() to the current CodeBlock. We pin the probed
// value at a single source location; across the recompile the same probe id
// must keep getting a fresh, non-empty dump, not the stale one.

load("./resources/iro-test-helpers.js", "caller relative");

function fn(arr) {
    return $vm.probe("p", arr.length - 1);
}
noInline(fn);

for (let i = 0; i < testLoopCount; ++i) fn([1, 2, 3]);

const dumpA = $vm.iroFactDump(fn);
if (!dumpA || typeof dumpA !== "object")
    throw new Error("monomorphic warm-up produced an empty IRO dump");

const compilesA = numberOfDFGCompiles(fn);

// Force an OSR exit by feeding a non-array shape, then re-monomorphize.
for (let i = 0; i < 1000; ++i) fn({ length: i });
for (let i = 0; i < testLoopCount * 10; ++i) fn([1, 2, 3]);

const compilesB = numberOfDFGCompiles(fn);
if (compilesB <= compilesA)
    throw new Error("expected at least one re-DFG-compile after shape variation; "
        + "before=" + compilesA + " after=" + compilesB);

const dumpB = $vm.iroFactDump(fn);
if (!dumpB || typeof dumpB !== "object")
    throw new Error("re-compile produced an empty IRO dump (FTL may not have caught up — reheat insufficient?)");

// The graph dump embeds the CodeBlock's pointers, so a fresh CodeBlock yields
// different text. If they match, iroFactDump is reading the stale CodeBlock.
if (dumpA.graph === dumpB.graph)
    throw new Error("dump did not change across re-compile — $vm.iroFactDump may be reading a stale CodeBlock");

// Both dumps should expose probe id="p" as the (arr.length - 1) value.
for (const [label, obj] of [["dumpA", dumpA], ["dumpB", dumpB]]) {
    const got = obj.probes && obj.probes.find(p => p.id === "p");
    if (!got)
        throw new Error(label + " is missing probe id=\"p\": " + JSON.stringify(obj));
}
