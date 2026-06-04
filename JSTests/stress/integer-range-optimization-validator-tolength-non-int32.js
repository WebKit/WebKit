//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Array.prototype.some is implemented in JS and calls @toLength on the
// length. With length = 2^60, ToLength's child is a non-Int32 node. IRO's
// ToLength case must skip such children so the validator's Int32 invariant
// holds; otherwise insertSingleRelationshipAssertion's RELEASE_ASSERT fires.

const ftlTrue = $vm.ftlTrue;
let didFTLCompile = 0;

const BIG = Math.pow(2, 60);
const evil = { 0: 'x', length: BIG };
const isX = v => v === 'x';

function doSome(arr, predicate) {
    didFTLCompile |= ftlTrue();
    return Array.prototype.some.call(arr, predicate);
}
noInline(doSome);

for (let i = 0; i < testLoopCount; ++i) {
    if (doSome(evil, isX) !== true)
        throw new Error("doSome should have returned true at iteration " + i);
}

if (!didFTLCompile)
    throw new Error("doSome never reached FTL - ToLength on Int52/Double length path untested");
