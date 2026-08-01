//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Lie to IRO via iroFactPoison, then call with a non-conforming value. The
// AssertInBounds emitted at the IROFactPoison site must catch the lie.

function poisoned(x) {
    let y = $vm.iroFactPoison(x | 0, 0, 0);
    return y + 5;
}
noInline(poisoned);

for (let i = 0; i < testLoopCount; ++i)
    poisoned(0);

poisoned(1000);

throw new Error("Should have crashed before reaching here");
