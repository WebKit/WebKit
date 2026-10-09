//@ skip if $secAccel3Removed
//@ runDefault("--useConcurrentJIT=false", "--thresholdForOptimizeAfterWarmUp=50", "--thresholdForOptimizeSoon=50", "--thresholdForFTLOptimizeAfterWarmUp=200", "--thresholdForFTLOptimizeSoon=200")

// A GetByVal inline cache runs inside its FTL frame with the caller's registers still live, so
// every register one of its cases touches has to be declared clobbered on the patchpoint. The
// float typed array cases box their result through AssemblyHelpers::purifyNaN, which writes the
// macro assembler's FP scratch (q31 on ARM64, xmm15 on x86_64). Nothing holds that register back
// from Air, so an undeclared write to it replaces a live double with PNaN: wrong answers, and no
// crash to point at.
//
// getSum keeps enough doubles live across a polymorphic o[INDEX] to fill every FP register and
// only adds them up afterwards, so whichever one Air parks in the FP scratch poisons the sum.

const LIVE_DOUBLES = 40;
const INDEX = 3;

let source = "";
for (let i = 0; i < LIVE_DOUBLES; ++i)
    source += `const a${i} = src[${i}];\n`;
source += `const v = o[${INDEX}];\n`;
source += "return v";
for (let i = 0; i < LIVE_DOUBLES; ++i)
    source += ` + a${i}`;
source += ";\n";

const getSum = new Function("src", "o", source);
noInline(getSum);

const src = new Float64Array(LIVE_DOUBLES);
for (let i = 0; i < LIVE_DOUBLES; ++i)
    src[i] = i + 0.5;

// A typed array plus a plain object keeps the GetByVal generic: MultiGetByVal covers neither
// NonArray indexing modes nor a mix it cannot fold, so the node keeps its inline cache instead of
// becoming a specialized typed array load. Both hold the same value at INDEX, so a call's result
// does not depend on which one it saw.
const typedArray = new Float64Array(LIVE_DOUBLES);
typedArray[INDEX] = 7.25;
const plainObject = {};
plainObject[INDEX] = 7.25;

const expected = getSum(src, typedArray);
if (getSum(src, plainObject) !== expected)
    throw new Error("broken test: the two shapes must produce the same sum");

for (let i = 0; i < testLoopCount; ++i) {
    const actual = getSum(src, (i % 8) ? typedArray : plainObject);
    if (actual !== expected)
        throw new Error(`iteration ${i} produced ${actual}, expected ${expected}`);
}
