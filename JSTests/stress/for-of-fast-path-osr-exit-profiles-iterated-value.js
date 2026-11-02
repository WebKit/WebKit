//@ skip if not $jitTests
//@ $skipModes << :lockdown
//@ requireOptions("--forceUnlinkedDFG=0", "--maximumFunctionForCallInlineCandidateBytecodeCostForDFG=500", "--maximumFunctionForCallInlineCandidateBytecodeCostForFTL=500")

// Only optimized code ever iterates a number, so a recompile can learn about it only from OSR exit profiling.

const strings = ["a", "bb", "ccc"];
const withNumber = ["a", "bb", 0];
const stringMap = new Map(strings.map(s => [s, s]));
const withNumberMap = new Map(withNumber.map(v => [v, v]));
const stringSet = new Set(strings);
const withNumberSet = new Set(withNumber);

function pick(inDFG, a, b)
{
    return inDFG ? b : a;
}
noInline(pick);

function lengthOrValue(v)
{
    return typeof v === "number" ? v : v.length;
}

function walkArray()
{
    let count = 0;
    for (let v of pick($vm.dfgTrue(), strings, withNumber))
        count += lengthOrValue(v);
    return count;
}
noInline(walkArray);

function walkMapKeys()
{
    let count = 0;
    for (let v of pick($vm.dfgTrue(), stringMap, withNumberMap).keys())
        count += lengthOrValue(v);
    return count;
}
noInline(walkMapKeys);

function walkMapValues()
{
    let count = 0;
    for (let v of pick($vm.dfgTrue(), stringMap, withNumberMap).values())
        count += lengthOrValue(v);
    return count;
}
noInline(walkMapValues);

function walkSet()
{
    let count = 0;
    for (let v of pick($vm.dfgTrue(), stringSet, withNumberSet))
        count += lengthOrValue(v);
    return count;
}
noInline(walkSet);

// requireOptions raises the inlining cost limits so that this for-of loop can be inlined.
function walkArrayInlinee(inDFG)
{
    let count = 0;
    for (let v of pick(inDFG, strings, withNumber))
        count += lengthOrValue(v);
    return count;
}

// The inlinee sees a number only when inlined into optimized code.
function walkArrayInlined()
{
    return walkArrayInlinee($vm.dfgTrue());
}
noInline(walkArrayInlined);

for (const walk of [walkArray, walkMapKeys, walkMapValues, walkSet, walkArrayInlined]) {
    for (let i = 0; i < 100000; ++i) {
        const result = walk();
        if (result !== 6 && result !== 3)
            throw new Error(`${walk.name}: bad result: ${result}`);
    }
    const compiles = numberOfDFGCompiles(walk);
    if (compiles > 4)
        throw new Error(`${walk.name} was DFG-compiled ${compiles} times`);
}
