//@ mustCrashWith!(:trap, "iroFactPoison: bound must be an integer in int32 range")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Non-integer bound - parser-side RELEASE_ASSERT must fire.

function misuse(x) {
    return $vm.iroFactPoison(x | 0, 0, 1.5);
}
noInline(misuse);

for (let i = 0; i < testLoopCount; ++i)
    misuse(i);

throw new Error("Should have crashed before reaching here");
