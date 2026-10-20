//@ skip if $buildType == "debug" or not $jitTests
//@ runFTLNoCJIT("--jitPolicyScale=0.001", "--maxDFGNodesInBasicBlockForPreciseAnalysis=1000", "--maxPerThreadStackUsage=1073741824")

function runNearStackLimit(f) {
    function t() {
        try {
            return t();
        } catch (e) {
            return f();
        }
    }
    return t();
}

const length = 1200;
const test = new Function("value", `
    let object = { next: value };
    ${"object = { next: object };".repeat(length - 1)}
    return object;
`);
noInline(test);

let object = runNearStackLimit(() => {
    let result;
    for (let i = 0; i < testLoopCount; i++)
        result = test(i);
    return result;
});

for (let i = 0; i < length; i++)
    object = object.next;
if (object !== testLoopCount - 1)
    throw new Error(object);
