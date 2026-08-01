//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Exposes the inserted AssertInBounds to LICM / CSE / DCE: the consumer is
// in-loop, so any post-IRO pass silently dropping a NodeMustGenerate
// AssertInBounds would make this test stop crashing.
function poisoned(start) {
    let acc = 0;
    for (let i = 0; i < 100; ++i) {
        let y = $vm.iroFactPoison(start | 0, 0, 0);
        acc = acc + y + 5;
    }
    return acc;
}
noInline(poisoned);

for (let i = 0; i < testLoopCount; ++i)
    poisoned(0);

poisoned(1000);

throw new Error("Should have crashed before reaching here");
