//@ mustCrashWith!(:trap, "iroFactPoison: bound must be a numeric constant")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Non-numeric bound - parser-side RELEASE_ASSERT must fire.

function misuse(x) {
    return $vm.iroFactPoison(x | 0, "low", "high");
}
noInline(misuse);

for (let i = 0; i < testLoopCount; ++i)
    misuse(i);

throw new Error("Should have crashed before reaching here");
