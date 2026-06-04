//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Truthful counterpart of the various *-crashes tests: every IRO transform
// that consults a poisoned range fact must FTL-compile cleanly and return
// correct values when the claim holds.
//
// Each function sticky-records whether FTL ever compiled it via $vm.ftlTrue().
// Without that check, a harness/loop-count regression that prevents FTL from
// engaging would make this whole file pass vacuously: IROFactPoison collapses
// to Identity outside FTL, so no assertion path runs.

const ftlTrue = $vm.ftlTrue;

let didFTLCompile = {
    viaArithAdd: 0,
    viaArithSub: 0,
    viaArithMul: 0,
    viaArithAbs: 0,
    viaCheckInBounds: 0,
};

function assert(actual, expected, label) {
    if (actual !== expected)
        throw new Error(label + ": expected " + expected + ", got " + actual);
}

function viaArithAdd(x) {
    didFTLCompile.viaArithAdd |= ftlTrue();
    return $vm.iroFactPoison(x | 0, 0, 100) + 5;
}
noInline(viaArithAdd);

function viaArithSub(x) {
    didFTLCompile.viaArithSub |= ftlTrue();
    return 200 - $vm.iroFactPoison(x | 0, 0, 100);
}
noInline(viaArithSub);

function viaArithMul(x) {
    didFTLCompile.viaArithMul |= ftlTrue();
    return $vm.iroFactPoison(x | 0, 0, 100) * 3;
}
noInline(viaArithMul);

function viaArithAbs(x) {
    didFTLCompile.viaArithAbs |= ftlTrue();
    return Math.abs($vm.iroFactPoison(x | 0, 0, 100));
}
noInline(viaArithAbs);

const array = new Array(128);
for (let i = 0; i < array.length; ++i)
    array[i] = i * 2;
function viaCheckInBounds(x) {
    didFTLCompile.viaCheckInBounds |= ftlTrue();
    let i = $vm.iroFactPoison(x | 0, 0, 127);
    return array[i];
}
noInline(viaCheckInBounds);

for (let i = 0; i < testLoopCount; ++i) {
    viaArithAdd(7);
    viaArithSub(7);
    viaArithMul(7);
    viaArithAbs(7);
    viaCheckInBounds(7);
}

assert(viaArithAdd(7), 12, "viaArithAdd");
assert(viaArithAdd(0), 5, "viaArithAdd boundary low");
assert(viaArithAdd(100), 105, "viaArithAdd boundary high");

assert(viaArithSub(7), 193, "viaArithSub");
assert(viaArithSub(0), 200, "viaArithSub boundary low");
assert(viaArithSub(100), 100, "viaArithSub boundary high");

assert(viaArithMul(7), 21, "viaArithMul");
assert(viaArithMul(0), 0, "viaArithMul boundary low");
assert(viaArithMul(100), 300, "viaArithMul boundary high");

assert(viaArithAbs(7), 7, "viaArithAbs");
assert(viaArithAbs(0), 0, "viaArithAbs boundary low");
assert(viaArithAbs(100), 100, "viaArithAbs boundary high");

assert(viaCheckInBounds(7), 14, "viaCheckInBounds");
assert(viaCheckInBounds(0), 0, "viaCheckInBounds boundary low");
assert(viaCheckInBounds(127), 254, "viaCheckInBounds boundary high");

for (const name of ["viaArithAdd", "viaArithSub", "viaArithMul", "viaArithAbs", "viaCheckInBounds"]) {
    if (!didFTLCompile[name])
        throw new Error(name + " never reached FTL - IRO assertion paths untested, coverage is vacuous");
}
