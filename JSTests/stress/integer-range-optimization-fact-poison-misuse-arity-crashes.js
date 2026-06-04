//@ mustCrashWith!(:trap, "$vm.iroFactPoison requires 3 arguments")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Too-few-args misuse - parser-side RELEASE_ASSERT must fire.

function misuse(x) {
    return $vm.iroFactPoison(x | 0);
}
noInline(misuse);

for (let i = 0; i < testLoopCount; ++i)
    misuse(i);

throw new Error("Should have crashed before reaching here");
