//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Drives IRO's ArithAbs -> Identity transform: the inserted range assertion
// must catch a value outside the poisoned [min, max].
function poisoned(x) {
    let y = $vm.iroFactPoison(x | 0, 0, 0);
    return Math.abs(y);
}
noInline(poisoned);

for (let i = 0; i < testLoopCount; ++i)
    poisoned(0);

poisoned(-1);

throw new Error("Should have crashed before reaching here");
