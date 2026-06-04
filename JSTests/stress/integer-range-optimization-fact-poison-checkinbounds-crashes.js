//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Drives the GetByVal-on-Undecided-array transform (converts to `undefined`
// once IRO proves the index is non-negative). The inserted range assertion
// must catch a negative runtime index.
const undecided = new Array(8);

function poisoned(x) {
    let i = $vm.iroFactPoison(x | 0, 0, 1000);
    return undecided[i];
}
noInline(poisoned);

for (let i = 0; i < testLoopCount; ++i)
    poisoned(0);

poisoned(-1);

throw new Error("Should have crashed before reaching here");
