//@ mustCrashWith!(:trap, "Bounds Check Elimination error found")
//@ skip if $hostOS == "windows"
//@ skip if !$jitTests
//@ runFTLNoCJIT("--useDollarVM=true", "--validateIntegerRangeOptimization=true")

// Two CheckInBounds in one function: the second is eliminable once the first
// adds `i > -1` / `i < length` facts and the poison adds a [0, 32] range.
// Even after that elimination, the validator must catch a runtime value
// outside the poisoned range.
const arr = new Array(64);
for (let i = 0; i < arr.length; ++i) arr[i] = i;

function probe(x) {
    let i = $vm.iroFactPoison(x | 0, 0, 32);
    let a = arr[i];
    let b = arr[i];
    return a + b;
}
noInline(probe);

for (let i = 0; i < testLoopCount; ++i)
    probe(0);

probe(99999);

throw new Error("Should have crashed before reaching here");
