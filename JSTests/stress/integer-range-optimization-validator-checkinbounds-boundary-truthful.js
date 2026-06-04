//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

const ftlTrue = $vm.ftlTrue;
// Use `|= ftlTrue()` not `if (ftlTrue()) ... = true`. The branch form gives DFG
// a profile where the true arm is never taken (DFG sees ftlTrue()===0), so FTL
// OSR-exits at the first true return and the AssertInBounds path never runs.
let didFTLCompile = 0;

const arr = new Array(64);
for (let i = 0; i < arr.length; ++i)
    arr[i] = i * 2;

function probe(x) {
    didFTLCompile |= ftlTrue();
    let i = x | 0;
    if (i < 0 || i >= arr.length) return -1;
    return arr[i];
}
noInline(probe);

for (let i = 0; i < testLoopCount; ++i)
    probe(0);

if (probe(63) !== 126)
    throw new Error("expected 126 at boundary, got " + probe(63));
if (probe(0) !== 0)
    throw new Error("expected 0 at low boundary, got " + probe(0));
if (probe(64) !== -1)
    throw new Error("expected -1 past boundary, got " + probe(64));

if (!didFTLCompile)
    throw new Error("probe never reached FTL - CheckInBounds var-var assertion path untested");
