//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// The false arm of `x === 5` gives IRO a `x != 5` fact; combined with a
// poisoned range, the validator emits AssertInBounds for both fact kinds.
// All facts hold - FTL compiles cleanly and results are correct.

const ftlTrue = $vm.ftlTrue;
// Use `|= ftlTrue()` not `if (ftlTrue()) ... = true`. The branch form gives DFG
// a profile where the true arm is never taken (DFG sees ftlTrue()===0), so FTL
// OSR-exits at the first true return and the AssertInBounds path never runs.
let didFTLCompile = 0;

function assert(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + ", got " + actual);
}

function viaNotEqual(x) {
    didFTLCompile |= ftlTrue();
    if (x === 5)
        return -1;
    return $vm.iroFactPoison(x | 0, 0, 100) + 1;
}
noInline(viaNotEqual);

for (let i = 0; i < testLoopCount; ++i)
    viaNotEqual(i % 4 === 0 ? 5 : (i & 63));

assert(viaNotEqual(5), -1, "viaNotEqual sentinel");
assert(viaNotEqual(0), 1, "viaNotEqual low");
assert(viaNotEqual(7), 8, "viaNotEqual normal");
assert(viaNotEqual(100), 101, "viaNotEqual high");

if (!didFTLCompile)
    throw new Error("viaNotEqual never reached FTL - NotEqual assertion path untested");
