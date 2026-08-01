//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Signed-negative range - the assertion must use signed comparison.
function poisoned(x) {
    let y = $vm.iroFactPoison(x | 0, -10, -1);
    return y + 1;
}
noInline(poisoned);

for (let i = 0; i < testLoopCount; ++i)
    poisoned(-3);

poisoned(5);

throw new Error("Should have crashed before reaching here");
